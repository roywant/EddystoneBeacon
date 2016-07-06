/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed-drivers/mbed.h"
#include "ble/BLE.h"
#include "EddystoneService.h"

#include "PersistentStorageHelper/ConfigParamsPersistence.h"
// #include "stdio.h"

// Instantiation of the main event loop for this program

#ifdef YOTTA_CFG_MBED_OS  // use minar on mbed OS
#include "EventQueue/EventQueueMinar.h"
typedef eq::EventQueueMinar event_queue_t;

#else      // otherwise use the event classic queue
#include "EventQueue/EventQueueClassic.h"
typedef eq::EventQueueClassic<
    /* event count */ 10
> event_queue_t;

#endif

static event_queue_t eventQueue;

EddystoneService *eddyServicePtr;

/* Duration after power-on that config service is available. */
static const int CONFIG_ADVERTISEMENT_TIMEOUT_SECONDS = YOTTA_CFG_EDDYSTONE_DEFAULT_CONFIG_ADVERTISEMENT_TIMEOUT_SECONDS;

/* Values for ADV packets related to firmware levels, calibrated based on measured values at 1m */
static const PowerLevels_t advTxPowerLevels = YOTTA_CFG_EDDYSTONE_DEFAULT_ADV_TX_POWER_LEVELS;
/* Values for radio power levels, provided by manufacturer. */
static const PowerLevels_t radioTxPowerLevels = YOTTA_CFG_EDDYSTONE_DEFAULT_RADIO_TX_POWER_LEVELS;

// This allows a quick switch between targets without changing the 'platform'
// settings in config.json each time. If you do change target to nrf51dk-gcc,
// note you will still need to change 'softdevice' to 's130'
//
 #define QUICK_SWITCH_TO_NRF51DK 1
#ifdef QUICK_SWITCH_TO_NRF51DK
    #define LED_OFF 1
    #define CONFIG_LED LED3
    #define SHUTDOWN_LED LED1
    #define RESET_BUTTON BUTTON1
#else
    #define LED_OFF YOTTA_CFG_PLATFORM_LED_OFF
    #define CONFIG_LED YOTTA_CFG_PLATFORM_CONFIG_LED
    #define SHUTDOWN_LED YOTTA_CFG_PLATFORM_SHUTDOWN_LED
    #define RESET_BUTTON YOTTA_CFG_PLATFORM_RESET_BUTTON
#endif


DigitalOut configLED(CONFIG_LED, LED_OFF);
DigitalOut shutdownLED(SHUTDOWN_LED, LED_OFF);
InterruptIn button(RESET_BUTTON);

static int buttonBusy;                                    // semaphore to make prevent switch bounce problems

static const int BLINKY_MSEC = 500;                       // How long to cycle config LED on/off
static int beaconIsOn = 1;                                // Button handler boolean to switch on or off
static event_queue_t::event_handle_t handle = 0;         // For the config mode timeout
static event_queue_t::event_handle_t BlinkyHandle = 0;   // For the blinking LED when in config mode

static void blinky(void)  { configLED = !configLED; }
static void shutdownLED_on(void) { shutdownLED = !LED_OFF; }
static void shutdownLED_off(void) { shutdownLED = LED_OFF; }
static void freeButtonBusy(void) { buttonBusy = false; }

static void configLED_on(void) {
    configLED = !LED_OFF;
    BlinkyHandle = eventQueue.post_every(blinky, BLINKY_MSEC);
}
static void configLED_off(void) {
    configLED = LED_OFF;
    if (BlinkyHandle) {
        eventQueue.cancel(BlinkyHandle);
        BlinkyHandle = NULL;
    }
}

/**
 * Callback triggered some time after application started to switch to beacon mode.
 */
static void timeoutToStartEddystoneBeaconAdvertisements(void)
{
    Gap::GapState_t state;
    state = BLE::Instance().gap().getState();
    if (!state.connected) { /* don't switch if we're in a connected state. */
        eddyServicePtr->startEddystoneBeaconAdvertisements();
        configLED_off();
    }
}

/**
 * Callback triggered for a connection event.
 */
static void connectionCallback(const Gap::ConnectionCallbackParams_t *cbParams)
{
    (void) cbParams;
    // Stop advertising whatever the current mode
    eddyServicePtr->stopEddystoneBeaconAdvertisements();
}

/**
 * Callback triggered for a disconnection event.
 */
static void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *cbParams)
{
    (void) cbParams;
    BLE::Instance().gap().startAdvertising();
    // Save params in persistent storage
    EddystoneService::EddystoneParams_t params;
    eddyServicePtr->getEddystoneParams(params);
    saveEddystoneServiceConfigParams(&params);
    // Ensure LED is off at the end of Config Mode or during a connection
    configLED_off();
    // 0.5 Second callback to rapidly re-establish Beaconing Service
    // (because it needs to be executed outside of disconnect callback)
    eventQueue.post_in(timeoutToStartEddystoneBeaconAdvertisements, 500 /* ms */);
}


// Callback used to handle button presses from thread mode (not IRQ)
static void button_task(void) {
    eventQueue.cancel(handle);   // kill any pending callback tasks

    if (beaconIsOn) {
        beaconIsOn = 0;
        eddyServicePtr->stopEddystoneBeaconAdvertisements();
        configLED_off();    // just in case it's still running...
        shutdownLED_on();   // Flash shutdownLED to let user know we're turning off
        eventQueue.post_in(shutdownLED_off, 1000);
    } else {

        beaconIsOn = 1;
        eddyServicePtr->startEddystoneConfigAdvertisements();
        configLED_on();
        handle = eventQueue.post_in(
            timeoutToStartEddystoneBeaconAdvertisements,
            CONFIG_ADVERTISEMENT_TIMEOUT_SECONDS * 1000 /* ms */
        );
    }
    eventQueue.post_in(freeButtonBusy, 750 /* ms */);
}

/**
 * Raw IRQ handler for the reset button. We don't want to actually do any work here.
 * Instead, we queue work to happen later using an event queue, by posting a callback.
 * This has the added avantage of serialising actions, so if the button press happens
 * during the config->beacon mode transition timeout, the button_task won't happen
 * until the previous task has finished.
 *
 * If your buttons aren't debounced, you should do this in software, or button_task
 * might get queued multiple times.
 */
static void reset_rise(void)
{
    if (!buttonBusy) {
        buttonBusy = true;
        eventQueue.post(button_task);
    }
}

static void onBleInitError(BLE::InitializationCompleteCallbackContext* initContext)
{
    /* Initialization error handling goes here... */
    (void) initContext;
}


static void bleInitComplete(BLE::InitializationCompleteCallbackContext* initContext)
{
    BLE         &ble  = initContext->ble;
    ble_error_t error = initContext->error;

    if (error != BLE_ERROR_NONE) {
        onBleInitError(initContext);
        return;
    }

    ble.gap().onDisconnection(disconnectionCallback);

    ble.gap().onConnection(connectionCallback);

    EddystoneService::EddystoneParams_t params;

    // Determine if booting directly after re-Flash or not
    if (loadEddystoneServiceConfigParams(&params)) {
        // 2+ Boot after reflash, so get parms from Persistent Storage
        eddyServicePtr = new EddystoneService(ble, params, radioTxPowerLevels, eventQueue);
    } else {
        // 1st Boot after reflash, so reset everything to defaults
        /* NOTE: slots are initialized in the constructor from the config.json file */
        eddyServicePtr = new EddystoneService(ble, advTxPowerLevels, radioTxPowerLevels, eventQueue);
    }

    // Save Default params in persistent storage ready for next boot event
    eddyServicePtr->getEddystoneParams(params);
    saveEddystoneServiceConfigParams(&params);

    // Start the Eddystone Config service - This will never stop (only connectability will change)
    eddyServicePtr->startEddystoneConfigService();

    /* Start Eddystone config Advertizements (to initialize everything properly) */
    configLED_on();
    eddyServicePtr->startEddystoneConfigAdvertisements();
    handle = eventQueue.post_in(
        timeoutToStartEddystoneBeaconAdvertisements,
        CONFIG_ADVERTISEMENT_TIMEOUT_SECONDS * 1000 /* ms */
    );

   // now shut everything off (used for final beacon that ships w/ battery)
   eventQueue.post_in(button_task, 2000 /* ms */);
}

void app_start(int, char *[])
{
    /* Tell standard C library to not allocate large buffers for these streams */
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    setbuf(stdin, NULL);

    beaconIsOn = 1;             // Booting up, initialize for button handler
    buttonBusy = false;         // software debouncing of the reset button
    button.rise(&reset_rise);   // setup reset button

    BLE &ble = BLE::Instance();
    ble.init(bleInitComplete);
}

#if !defined(YOTTA_CFG_MBED_OS)

int main() {

    app_start(0, NULL);

    while (true) {
       eventQueue.dispatch();
       BLE::Instance().waitForEvent();
    }

    return 0;
}

#endif
