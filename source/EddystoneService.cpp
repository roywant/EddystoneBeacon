/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "EddystoneService.h"


/* Use define zero for production, 1 for testing to allow connection at any time */
#define DEFAULT_REMAIN_CONNECTABLE 0x01

const char * const EddystoneService::slotDefaultUrls[] = YOTTA_CFG_EDDYSTONE_DEFAULT_SLOT_URLS;

/* 
 * CONSTRUCTOR #1 Used on 1st boot (after reflash)
 */
EddystoneService::EddystoneService(BLE                 &bleIn,
                                   const PowerLevels_t &advTxPowerLevelsIn,
                                   const PowerLevels_t &radioTxPowerLevelsIn,
                                   uint32_t            advConfigIntervalIn) :
    ble(bleIn),
    operationMode(EDDYSTONE_MODE_NONE),
    uidFrame(),
    urlFrame(),
    tlmFrame(),
    eidFrame(),
    tlmBatteryVoltageCallback(NULL),
    tlmBeaconTemperatureCallback(NULL),
    radioManagerCallbackHandle(NULL),
    deviceName(DEFAULT_DEVICE_NAME)
{

    if (advConfigIntervalIn != 0) {
        if (advConfigIntervalIn < ble.gap().getMinAdvertisingInterval()) {
            advConfigInterval = ble.gap().getMinAdvertisingInterval();
        } else if (advConfigIntervalIn > ble.gap().getMaxAdvertisingInterval()) {
            advConfigInterval = ble.gap().getMaxAdvertisingInterval();
        } else {
            advConfigInterval = advConfigIntervalIn;
        }
    }
    memcpy(radioTxPowerLevels, radioTxPowerLevelsIn, sizeof(PowerLevels_t));
    memcpy(advTxPowerLevels,   advTxPowerLevelsIn,   sizeof(PowerLevels_t));
    
    doFactoryReset();

    /* TODO: Note that this timer is started from the time EddystoneService
     * is initialised and NOT from when the device is booted.
     */
    timeSinceBootTimer.start();

    /* Set the device name at startup */
    ble.gap().setDeviceName(reinterpret_cast<const uint8_t *>(deviceName));
}

/* 
 * Constuctor #2:  Used on 2nd+ boot: EddystoneService parameters derived from persistent storage 
 */
EddystoneService::EddystoneService(BLE                 &bleIn,
                                   EddystoneParams_t   &paramsIn,
                                   const PowerLevels_t &radioTxPowerLevelsIn,
                                   uint32_t            advConfigIntervalIn) :
    ble(bleIn),
    operationMode(EDDYSTONE_MODE_NONE),
    uidFrame(),
    urlFrame(),
    tlmFrame(),
    eidFrame(),
    tlmBatteryVoltageCallback(NULL),
    tlmBeaconTemperatureCallback(NULL),
    radioManagerCallbackHandle(NULL),
    deviceName(DEFAULT_DEVICE_NAME)
{   
    memcpy(capabilities, paramsIn.capabilities, sizeof(Capability_t));
    activeSlot          = paramsIn.activeSlot;
    memcpy(radioTxPowerLevels, radioTxPowerLevelsIn, sizeof(PowerLevels_t));
    memcpy(slotRadioTxPowerLevels, paramsIn.slotRadioTxPowerLevels, sizeof(SlotTxPowerLevels_t));    
    memcpy(advTxPowerLevels,   paramsIn.advTxPowerLevels,   sizeof(PowerLevels_t));
    memcpy(slotAdvTxPowerLevels, paramsIn.slotAdvTxPowerLevels, sizeof(SlotTxPowerLevels_t));
    memcpy(slotAdvIntervals,   paramsIn.slotAdvIntervals,   sizeof(SlotAdvIntervals_t));
    lockState           = paramsIn.lockState;
    memcpy(unlockKey,   paramsIn.unlockKey,   sizeof(Lock_t));
    memcpy(unlockToken, paramsIn.unlockToken, sizeof(Lock_t));
    memcpy(challenge, paramsIn.challenge, sizeof(Lock_t));
    memset(slotCallbackHandles, 0, sizeof(SlotCallbackHandles_t));
    memcpy(slotStorage, paramsIn.slotStorage, sizeof(SlotStorage_t)); 
    memcpy(slotFrameTypes, paramsIn.slotFrameTypes, sizeof(SlotFrameTypes_t));
    memcpy(slotEidRotationPeriodExps, paramsIn.slotEidRotationPeriodExps, sizeof(SlotEidRotationPeriodExps_t));
    memcpy(slotEidIdentityKeys, paramsIn.slotEidIdentityKeys, sizeof(SlotEidIdentityKeys_t));
    remainConnectable   = paramsIn.remainConnectable;
    
    if (advConfigIntervalIn != 0) {
        if (advConfigIntervalIn < ble.gap().getMinAdvertisingInterval()) {
            advConfigInterval = ble.gap().getMinAdvertisingInterval();
        } else if (advConfigIntervalIn > ble.gap().getMaxAdvertisingInterval()) {
            advConfigInterval = ble.gap().getMaxAdvertisingInterval();
        } else {
            advConfigInterval = advConfigIntervalIn;
        }
    }
    
    // Generate fresh private and public ECDH keys for EID
    eidFrame.genBeaconKeys(privateEcdhKey, publicEcdhKey);
    
    // Recompute EID Slot Data
    for (int slot = 0; slot < MAX_ADV_SLOTS; slot++) {
        uint8_t* frame = slotToFrame(slot);
        switch (slotFrameTypes[slot]) {
            case EDDYSTONE_FRAME_EID:
               eidFrame.update(frame, slotEidIdentityKeys[slot], slotEidRotationPeriodExps[slot], timeSinceBootTimer.read_ms() / 1000);
               eidFrame.setAdvTxPower(frame, slotAdvTxPowerLevels[slot]);
               break;       
            default: ;
        }
    }
    

    /* TODO: Note that this timer is started from the time EddystoneService
     * is initialised and NOT from when the device is booted.
     */
    timeSinceBootTimer.start();

    /* Set the device name at startup */
    ble.gap().setDeviceName(reinterpret_cast<const uint8_t *>(deviceName));
}


/**
 * Factory reset all parmeters: used at initial boot, and activated from Char 11
 */
void EddystoneService::doFactoryReset(void)
{   
    memset(slotCallbackHandles, 0, sizeof(SlotCallbackHandles_t)); 
    radioManagerCallbackHandle = NULL; 
    memcpy(capabilities, CAPABILITIES_DEFAULT, CAP_HDR_LEN);
    // Line above leaves powerlevels blank; Line below fills them in
    memcpy(capabilities + CAP_HDR_LEN, radioTxPowerLevels, sizeof(PowerLevels_t));
    activeSlot = DEFAULT_SLOT;
    // Intervals
    uint16_t buf1[] = YOTTA_CFG_EDDYSTONE_DEFAULT_SLOT_INTERVALS;
    for (int i = 0; i < MAX_ADV_SLOTS; i++) {
            // Ensure all slot periods are in range
            buf1[i] = correctAdvertisementPeriod(buf1[i]);
    }
    memcpy(slotAdvIntervals, buf1, sizeof(SlotAdvIntervals_t));
    // Radio and Adv TX Power
    int8_t buf2[] = YOTTA_CFG_EDDYSTONE_DEFAULT_SLOT_TX_POWERS;
    for (int i = 0; i< MAX_ADV_SLOTS; i++) { 
      slotRadioTxPowerLevels[i] = buf2[i];
      slotAdvTxPowerLevels[i] = advTxPowerLevels[radioTxPowerToIndex(buf2[i])];
    }
    // Lock
    lockState      = UNLOCKED;
    uint8_t defKeyBuf[] = YOTTA_CFG_EDDYSTONE_DEFAULT_UNLOCK_KEY;
    memcpy(unlockKey,        defKeyBuf,     sizeof(Lock_t)); 
    memset(unlockToken,      0,     sizeof(Lock_t));
    memset(challenge,        0,     sizeof(Lock_t)); // NOTE: challenge is randomized on first unlockChar read; 
    
    // EID
    eidFrame.genBeaconKeys(privateEcdhKey, publicEcdhKey);
    //// TODO(AZ): recommend to randomly generate the default EIKs, instead of letting them be fixed
    // generateRandom(reinterpret_cast<uint8_t*>(slotEidIdentityKeys), sizeof(SlotEidIdentityKeys_t));
    memcpy(slotEidIdentityKeys, slotDefaultEidIdentityKeys, sizeof(SlotEidIdentityKeys_t));
    
    uint8_t buf4[] = YOTTA_CFG_EDDYSTONE_DEFAULT_SLOT_EID_ROTATION_PERIOD_EXPS;
    memcpy(slotEidRotationPeriodExps, buf4, sizeof(SlotEidRotationPeriodExps_t));
    memset(slotEidNextRotationTimes, 0, sizeof(SlotEidNextRotationTimes_t));
    //  Slot Data Type Defaults
    uint8_t buf3[] = YOTTA_CFG_EDDYSTONE_DEFAULT_SLOT_TYPES;
    memcpy(slotFrameTypes, buf3, sizeof(SlotFrameTypes_t));
    // Initialize Slot Data Defaults
    for (int slot = 0; slot < MAX_ADV_SLOTS; slot++) {
        uint8_t* frame = slotToFrame(slot);
        switch (slotFrameTypes[slot]) {
            case EDDYSTONE_FRAME_UID: 
               uidFrame.setData(frame, slotAdvTxPowerLevels[slot], reinterpret_cast<const uint8_t*>(slotDefaultUids[slot]));
               break;
            case EDDYSTONE_FRAME_URL:
               urlFrame.setUnencodedUrlData(frame, slotAdvTxPowerLevels[slot], slotDefaultUrls[slot]);
               break;
            case EDDYSTONE_FRAME_TLM: 
               tlmFrame.setTLMData(TLMFrame::DEFAULT_TLM_VERSION);
               tlmFrame.setData(frame);
               break;
            case EDDYSTONE_FRAME_EID:
               eidFrame.setData(frame, slotAdvTxPowerLevels[slot], reinterpret_cast<const uint8_t*>(allSlotsDefaultEid));            
               eidFrame.update(frame, slotEidIdentityKeys[slot], slotEidRotationPeriodExps[slot], timeSinceBootTimer.read_ms() / 1000);
               break;       
        }
    }
    remainConnectable = DEFAULT_REMAIN_CONNECTABLE;  
    factoryReset = false;
}

/* Setup callback to update BatteryVoltage in TLM frame */
void EddystoneService::onTLMBatteryVoltageUpdate(TlmUpdateCallback_t tlmBatteryVoltageCallbackIn)
{
    tlmBatteryVoltageCallback = tlmBatteryVoltageCallbackIn;
}

/* Setup callback to update BeaconTemperature in TLM frame */
void EddystoneService::onTLMBeaconTemperatureUpdate(TlmUpdateCallback_t tlmBeaconTemperatureCallbackIn)
{
    tlmBeaconTemperatureCallback = tlmBeaconTemperatureCallbackIn;
}

EddystoneService::EddystoneError_t EddystoneService::startEddystoneBeaconAdvertisements(void)
{   
    stopEddystoneBeaconAdvertisements();

    bool intervalValidFlag = false;
    for (int i = 0; i < MAX_ADV_SLOTS; i++) {
        if (slotAdvIntervals[i] != 0) {
            intervalValidFlag = true;
        }
    }
    
    if (!intervalValidFlag) {
        /* Nothing to do, the period is 0 for all frames */
        return EDDYSTONE_ERROR_INVALID_ADVERTISING_INTERVAL;
    }

    // In case left over from Config Adv Mode
    ble.gap().clearScanResponse(); 
        
    operationMode = EDDYSTONE_MODE_BEACON;
    
    /* Configure advertisements initially at power of active slot*/
    ble.gap().setTxPower(slotRadioTxPowerLevels[activeSlot]); 
    
    if (remainConnectable) {
        ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    } else {
         ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_NON_CONNECTABLE_UNDIRECTED);
    }
    ble.gap().setAdvertisingInterval(ble.gap().getMaxAdvertisingInterval());

    /* Make sure the queue is currently empty */
    advFrameQueue.reset();
    /* Setup callbacks to periodically add frames to be advertised to the queue and
     * add initial frame so that we have something to advertise on startup */
    for (int slot = 0; slot < MAX_ADV_SLOTS; slot++) {
        if (slotAdvIntervals[slot]) {
            advFrameQueue.push(slot);
            slotCallbackHandles[slot] = minar::Scheduler::postCallback(
                mbed::util::FunctionPointer1<void, int>(this, &EddystoneService::enqueueFrame).bind(slot)
            ).period(minar::milliseconds(slotAdvIntervals[slot])).getHandle();
        }
    }
    /* Start advertising */
    manageRadio();

    return EDDYSTONE_ERROR_NONE;
}

ble_error_t EddystoneService::setCompleteDeviceName(const char *deviceNameIn)
{
    /* Make sure the device name is safe */
    ble_error_t error = ble.gap().setDeviceName(reinterpret_cast<const uint8_t *>(deviceNameIn));
    if (error == BLE_ERROR_NONE) {
        deviceName = deviceNameIn;
        if (operationMode == EDDYSTONE_MODE_CONFIG) {
            /* Need to update the advertising packets to the new name */
            setupEddystoneConfigScanResponse();
        }
    }

    return error;
}

/* It is not the responsibility of the Eddystone implementation to store
 * the configured parameters in persistent storage since this is
 * platform-specific. So we provide this function that returns the
 * configured values that need to be stored and the main application
 * takes care of storing them.
 */
void EddystoneService::getEddystoneParams(EddystoneParams_t &params)
{
    // Capabilities
    memcpy(params.capabilities,     capabilities,           sizeof(Capability_t));
    // Active Slot
    params.activeSlot                = activeSlot;
    // Intervals
    memcpy(params.slotAdvIntervals, slotAdvIntervals,       sizeof(SlotAdvIntervals_t));
    // Power Levels
    memcpy(params.radioTxPowerLevels, radioTxPowerLevels,   sizeof(PowerLevels_t));
    memcpy(params.advTxPowerLevels,   advTxPowerLevels,     sizeof(PowerLevels_t));
    // Slot Power Levels
    memcpy(params.slotRadioTxPowerLevels, slotRadioTxPowerLevels,   sizeof(MAX_ADV_SLOTS));
    memcpy(params.slotAdvTxPowerLevels,   slotAdvTxPowerLevels,     sizeof(MAX_ADV_SLOTS));
    // Lock
    params.lockState                = lockState;
    memcpy(params.unlockKey,        unlockKey,              sizeof(Lock_t));
    memcpy(params.unlockToken,      unlockToken,            sizeof(Lock_t));
    memcpy(params.challenge,        challenge,              sizeof(Lock_t));
    // Slots
    memcpy(params.slotFrameTypes,   slotFrameTypes,         sizeof(SlotFrameTypes_t));
    memcpy(params.slotStorage,      slotStorage,            sizeof(SlotStorage_t));
    memcpy(params.slotEidRotationPeriodExps, slotEidRotationPeriodExps, sizeof(SlotEidRotationPeriodExps_t));
    memcpy(params.slotEidIdentityKeys, slotEidIdentityKeys, sizeof(SlotEidIdentityKeys_t));
    // Testing and Management
    params.remainConnectable        = remainConnectable;
}

void EddystoneService::swapAdvertisedFrame(int slot)
{       
    uint8_t* frame = slotToFrame(slot);
    uint8_t frameType = slotFrameTypes[slot];
    uint32_t timeSecs = timeSinceBootTimer.read_ms() / 1000;
    switch (frameType) {
        case EDDYSTONE_FRAME_UID:
            updateAdvertisementPacket(uidFrame.getAdvFrame(frame), uidFrame.getAdvFrameLength(frame));
            break;
        case EDDYSTONE_FRAME_URL:
            updateAdvertisementPacket(urlFrame.getAdvFrame(frame), urlFrame.getAdvFrameLength(frame));
            break;
        case EDDYSTONE_FRAME_TLM:
            updateRawTLMFrame(frame);
            updateAdvertisementPacket(tlmFrame.getAdvFrame(frame), tlmFrame.getAdvFrameLength(frame));
            break;
        case EDDYSTONE_FRAME_EID:
            // only update the frame if the rotation period is due
            if (timeSecs >= slotEidNextRotationTimes[slot]) {
                eidFrame.update(frame, slotEidIdentityKeys[slot], slotEidRotationPeriodExps[slot], timeSecs); 
                slotEidNextRotationTimes[slot] = timeSecs + (1 << slotEidRotationPeriodExps[slot]);
            }
            updateAdvertisementPacket(eidFrame.getAdvFrame(frame), eidFrame.getAdvFrameLength(frame));
            break;
        default:
            //Some error occurred 
            error("Frame to swap in does not specify a valid type");
            break;
    }
    ble.gap().setTxPower(slotRadioTxPowerLevels[slot]); 
}


/* Helper function that calls user-defined functions to update Battery Voltage and Temperature (if available),
 * then updates the raw frame data and finally updates the actual advertised packet. This operation must be
 * done fairly often because the TLM frame TimeSinceBoot must have a 0.1 secs resolution according to the
 * Eddystone specification.
 */
void EddystoneService::updateRawTLMFrame(uint8_t* frame)
{
    if (tlmBeaconTemperatureCallback != NULL) {
        tlmFrame.updateBeaconTemperature((*tlmBeaconTemperatureCallback)(tlmFrame.getBeaconTemperature()));
    }
    if (tlmBatteryVoltageCallback != NULL) {
        tlmFrame.updateBatteryVoltage((*tlmBatteryVoltageCallback)(tlmFrame.getBatteryVoltage()));
    }
    tlmFrame.updateTimeSinceBoot(timeSinceBootTimer.read_ms());
    tlmFrame.setData(frame);
}

void EddystoneService::updateAdvertisementPacket(const uint8_t* rawFrame, size_t rawFrameLength)
{
    ble.gap().clearAdvertisingPayload();
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, EDDYSTONE_UUID, sizeof(EDDYSTONE_UUID));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::SERVICE_DATA, rawFrame, rawFrameLength);
}

uint8_t* EddystoneService::slotToFrame(int slot)
{
   return reinterpret_cast<uint8_t *>(&slotStorage[slot * sizeof(Slot_t)]);
}

void EddystoneService::enqueueFrame(int slot)
{
    advFrameQueue.push(slot);
    if (!radioManagerCallbackHandle) {
        /* Advertising stopped and there is not callback posted in minar. Just
         * execute the manager to resume advertising */
        manageRadio();
    }
}

void EddystoneService::manageRadio(void)
{
    uint8_t slot;
    uint32_t  startTimeManageRadio = timeSinceBootTimer.read_ms();

    /* Signal that there is currently no callback posted */
    radioManagerCallbackHandle = NULL;

    if (advFrameQueue.pop(slot)) {
        /* We have something to advertise */
        if (ble.gap().getState().advertising) {
            ble.gap().stopAdvertising();
        }
        swapAdvertisedFrame(slot);
        ble.gap().startAdvertising();

        /* Increase the advertised packet count in TLM frame */
        tlmFrame.updatePduCount();

        /* Post a callback to itself to stop the advertisement or pop the next
         * frame from the queue. However, take into account the time taken to
         * swap in this frame. */
        radioManagerCallbackHandle = minar::Scheduler::postCallback(
            this,
            &EddystoneService::manageRadio
        ).delay(
            minar::milliseconds(ble.gap().getMinNonConnectableAdvertisingInterval() - (timeSinceBootTimer.read_ms() - startTimeManageRadio))
        ).tolerance(0).getHandle();
    } else if (ble.gap().getState().advertising) {
        /* Nothing else to advertise, stop advertising and do not schedule any callbacks */
        ble.gap().stopAdvertising();
    }
}

void EddystoneService::startEddystoneConfigService(void)
{
    uint16_t beAdvInterval = swapEndian(slotAdvIntervals[activeSlot]);
    int8_t radioTxPower = slotRadioTxPowerLevels[activeSlot];
    int8_t advTxPower = slotAdvTxPowerLevels[activeSlot];
    uint8_t* slotData = slotToFrame(activeSlot) + 1;
    aes128Encrypt(unlockKey, slotEidIdentityKeys[activeSlot], encryptedEidIdentityKey);
    
    capabilitiesChar      = new ReadOnlyArrayGattCharacteristic<uint8_t, sizeof(Capability_t)>(UUID_CAPABILITIES_CHAR, capabilities);
    activeSlotChar        = new ReadWriteGattCharacteristic<uint8_t>(UUID_ACTIVE_SLOT_CHAR, &activeSlot); 
    advIntervalChar       = new ReadWriteGattCharacteristic<uint16_t>(UUID_ADV_INTERVAL_CHAR, &beAdvInterval); 
    radioTxPowerChar      = new ReadWriteGattCharacteristic<int8_t>(UUID_RADIO_TX_POWER_CHAR, &radioTxPower); 
    advTxPowerChar        = new ReadWriteGattCharacteristic<int8_t>(UUID_ADV_TX_POWER_CHAR, &advTxPower); 
    lockStateChar         = new GattCharacteristic(UUID_LOCK_STATE_CHAR, &lockState, sizeof(uint8_t), sizeof(LockState_t), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE);
    unlockChar            = new ReadWriteArrayGattCharacteristic<uint8_t, sizeof(Lock_t)>(UUID_UNLOCK_CHAR, unlockToken);
    publicEcdhKeyChar     = new GattCharacteristic(UUID_PUBLIC_ECDH_KEY_CHAR, publicEcdhKey, 0, sizeof(PublicEcdhKey_t), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ);
    eidIdentityKeyChar    = new GattCharacteristic(UUID_EID_IDENTITY_KEY_CHAR, encryptedEidIdentityKey, 0, sizeof(EidIdentityKey_t), GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ);
    advSlotDataChar       = new GattCharacteristic(UUID_ADV_SLOT_DATA_CHAR, slotData, 0, MAX_SLOT_SIZE, GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE);
    factoryResetChar      = new WriteOnlyGattCharacteristic<uint8_t>(UUID_FACTORY_RESET_CHAR, &factoryReset);
    remainConnectableChar = new ReadWriteGattCharacteristic<uint8_t>(UUID_REMAIN_CONNECTABLE_CHAR, &remainConnectable);

    // CHAR-1 capabilities (READ ONLY)
    capabilitiesChar->setReadAuthorizationCallback(this, &EddystoneService::readBasicTestLockAuthorizationCallback);
    // CHAR-2 Active Slot
    activeSlotChar->setReadAuthorizationCallback(this, &EddystoneService::readBasicTestLockAuthorizationCallback);
    activeSlotChar->setWriteAuthorizationCallback(this, &EddystoneService::writeActiveSlotAuthorizationCallback<uint8_t>);
    // CHAR-3 Adv Interval
    advIntervalChar->setReadAuthorizationCallback(this, &EddystoneService::readAdvIntervalAuthorizationCallback);
    advIntervalChar->setWriteAuthorizationCallback(this, &EddystoneService::writeBasicAuthorizationCallback<uint16_t>);
    // CHAR-4  Radio TX Power
    radioTxPowerChar->setReadAuthorizationCallback(this, &EddystoneService::readRadioTxPowerAuthorizationCallback);
    radioTxPowerChar->setWriteAuthorizationCallback(this, &EddystoneService::writeBasicAuthorizationCallback<uint8_t>);
    // CHAR-5
    advTxPowerChar->setReadAuthorizationCallback(this, &EddystoneService::readAdvTxPowerAuthorizationCallback);
    advTxPowerChar->setWriteAuthorizationCallback(this, &EddystoneService::writeBasicAuthorizationCallback<uint8_t>);
    // CHAR-6 Lock State 
    lockStateChar->setWriteAuthorizationCallback(this, &EddystoneService::writeLockStateAuthorizationCallback);
    // CHAR-7 Unlock
    unlockChar->setReadAuthorizationCallback(this, &EddystoneService::readUnlockAuthorizationCallback);
    unlockChar->setWriteAuthorizationCallback(this, &EddystoneService::writeUnlockAuthorizationCallback);
    // CHAR-8 Public Ecdh Key
    publicEcdhKeyChar->setReadAuthorizationCallback(this, &EddystoneService::readBasicTestLockAuthorizationCallback);
    // publicEcdhKeyChar->setWriteAuthorizationCallback(this, &EddystoneService::writeBasicAuthorizationCallback<PublicEcdhKey_t>);
    // CHAR-9 EID Identity Key
    eidIdentityKeyChar->setReadAuthorizationCallback(this, &EddystoneService::readBasicTestLockAuthorizationCallback);
    // eidIdentityKeyChar->setWriteAuthorizationCallback(this, &EddystoneService::writeBasicAuthorizationCallback<EidIdentityKey_t>);
    // CHAR-10 Adv Slot Data
    advSlotDataChar->setReadAuthorizationCallback(this, &EddystoneService::readDataAuthorizationCallback);
    advSlotDataChar->setWriteAuthorizationCallback(this, &EddystoneService::writeVarLengthDataAuthorizationCallback);
    // CHAR-11 Factory Reset
    factoryResetChar->setReadAuthorizationCallback(this, &EddystoneService::readBasicTestLockAuthorizationCallback);
    factoryResetChar->setWriteAuthorizationCallback(this, &EddystoneService::writeBasicAuthorizationCallback<bool>);
    // CHAR-12 Remain Connectable
    remainConnectableChar->setReadAuthorizationCallback(this, &EddystoneService::readBasicTestLockAuthorizationCallback);
    remainConnectableChar->setWriteAuthorizationCallback(this, &EddystoneService::writeBasicAuthorizationCallback<bool>);

    // Create pointers to all characteristics in the GATT service
    charTable[0] = capabilitiesChar;
    charTable[1] = activeSlotChar;
    charTable[2] = advIntervalChar;
    charTable[3] = radioTxPowerChar;
    charTable[4] = advTxPowerChar;
    charTable[5] = lockStateChar;
    charTable[6] = unlockChar;
    charTable[7] = publicEcdhKeyChar;
    charTable[8] = eidIdentityKeyChar;
    charTable[9] = advSlotDataChar;
    charTable[10] = factoryResetChar;
    charTable[11] = remainConnectableChar;

    GattService configService(UUID_ES_BEACON_SERVICE, charTable, sizeof(charTable) / sizeof(GattCharacteristic *));

    ble.gattServer().addService(configService);
    ble.gattServer().onDataWritten(this, &EddystoneService::onDataWrittenCallback);
    updateCharacteristicValues();
}


void EddystoneService::freeConfigCharacteristics(void)
{
    delete capabilitiesChar;
    delete activeSlotChar;
    delete advIntervalChar;
    delete radioTxPowerChar;
    delete advTxPowerChar;
    delete lockStateChar;
    delete unlockChar;
    delete publicEcdhKeyChar;
    delete eidIdentityKeyChar;
    delete advSlotDataChar;
    delete factoryResetChar;
    delete remainConnectableChar;
}

void EddystoneService::stopEddystoneBeaconAdvertisements(void)
{
    /* Unschedule callbacks */
    
    for (int slot = 0; slot < MAX_ADV_SLOTS; slot++) {
        if (slotCallbackHandles[slot]) {
            minar::Scheduler::cancelCallback(slotCallbackHandles[slot]);
            slotCallbackHandles[slot] = NULL;
        }
    }
   
    if (radioManagerCallbackHandle) {
        minar::Scheduler::cancelCallback(radioManagerCallbackHandle);
        radioManagerCallbackHandle = NULL;
    }
    
    /* Stop any current Advs (ES Config or Beacon) */
    BLE::Instance().gap().stopAdvertising();
}

/*
 * Internal helper function used to update the GATT database following any
 * change to the internal state of the service object.
 */
void EddystoneService::updateCharacteristicValues(void)
{
    // Init variables for update
    uint16_t beAdvInterval = swapEndian(slotAdvIntervals[activeSlot]);
    int8_t radioTxPower = slotRadioTxPowerLevels[activeSlot];
    int8_t advTxPower = slotAdvTxPowerLevels[activeSlot];
    uint8_t* frame = slotToFrame(activeSlot);
    uint8_t slotLength = 0;
    uint8_t* slotData = NULL;
    memset(encryptedEidIdentityKey, 0, sizeof(encryptedEidIdentityKey));
    
    switch(slotFrameTypes[activeSlot]) {
        case EDDYSTONE_FRAME_UID:
          slotLength = uidFrame.getDataLength(frame); 
          slotData = uidFrame.getData(frame);
          break;
        case EDDYSTONE_FRAME_URL:
          slotLength = urlFrame.getDataLength(frame); 
          slotData = urlFrame.getData(frame);
          break;
        case EDDYSTONE_FRAME_TLM:
          updateRawTLMFrame(frame);
          slotLength = tlmFrame.getDataLength(frame);
          slotData = tlmFrame.getData(frame);
          break;
        case EDDYSTONE_FRAME_EID:
          slotLength = eidFrame.getDataLength(frame);
          slotData = eidFrame.getData(frame);
          aes128Encrypt(unlockKey, slotEidIdentityKeys[activeSlot], encryptedEidIdentityKey);
          break;
    }

    ble.gattServer().write(capabilitiesChar->getValueHandle(), reinterpret_cast<uint8_t *>(capabilities), sizeof(Capability_t));
    ble.gattServer().write(activeSlotChar->getValueHandle(), &activeSlot, sizeof(uint8_t));
    ble.gattServer().write(advIntervalChar->getValueHandle(), reinterpret_cast<uint8_t *>(&beAdvInterval), sizeof(uint16_t)); 
    ble.gattServer().write(radioTxPowerChar->getValueHandle(), reinterpret_cast<uint8_t *>(&radioTxPower), sizeof(int8_t));
    ble.gattServer().write(advTxPowerChar->getValueHandle(), reinterpret_cast<uint8_t *>(&advTxPower), sizeof(int8_t));
    ble.gattServer().write(lockStateChar->getValueHandle(), &lockState, sizeof(uint8_t));
    ble.gattServer().write(unlockChar->getValueHandle(), unlockToken, sizeof(Lock_t));
    ble.gattServer().write(publicEcdhKeyChar->getValueHandle(), reinterpret_cast<uint8_t *>(publicEcdhKey), sizeof(PublicEcdhKey_t));
    ble.gattServer().write(eidIdentityKeyChar->getValueHandle(), reinterpret_cast<uint8_t *>(encryptedEidIdentityKey), sizeof(EidIdentityKey_t));
    ble.gattServer().write(advSlotDataChar->getValueHandle(), slotData, slotLength);
    ble.gattServer().write(factoryResetChar->getValueHandle(), &factoryReset, sizeof(uint8_t));
    ble.gattServer().write(remainConnectableChar->getValueHandle(), &remainConnectable, sizeof(uint8_t));
}

EddystoneService::EddystoneError_t EddystoneService::startEddystoneConfigAdvertisements(void)
{
    stopEddystoneBeaconAdvertisements();
    
    if (advConfigInterval == 0) {
        // Nothing to do, the advertisement interval is 0 
        return EDDYSTONE_ERROR_INVALID_ADVERTISING_INTERVAL;
    }
    
    operationMode = EDDYSTONE_MODE_CONFIG;
    
    ble.gap().clearAdvertisingPayload();

    /* Accumulate the new payload */
    ble.gap().accumulateAdvertisingPayload(
        GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE
    );
    /* UUID is in different order in the ADV frame (!) */
    uint8_t reversedServiceUUID[sizeof(UUID_ES_BEACON_SERVICE)];
    for (size_t i = 0; i < sizeof(UUID_ES_BEACON_SERVICE); i++) {
        reversedServiceUUID[i] = UUID_ES_BEACON_SERVICE[sizeof(UUID_ES_BEACON_SERVICE) - i - 1];
    }
    ble.gap().accumulateAdvertisingPayload(
        GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS,
        reversedServiceUUID,
        sizeof(reversedServiceUUID)
    );
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::GENERIC_TAG);
    setupEddystoneConfigScanResponse();

    ble.gap().setTxPower(radioTxPowerLevels[sizeof(PowerLevels_t)-1]); // Max Power for Config
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(advConfigInterval);
    ble.gap().startAdvertising();
    
    return EDDYSTONE_ERROR_NONE;
}

void EddystoneService::setupEddystoneConfigScanResponse(void)
{
    ble.gap().clearScanResponse();
    ble.gap().accumulateScanResponse(
        GapAdvertisingData::COMPLETE_LOCAL_NAME,
        reinterpret_cast<const uint8_t *>(deviceName),
        strlen(deviceName)
    );
    ble.gap().accumulateScanResponse(
        GapAdvertisingData::TX_POWER_LEVEL,
        reinterpret_cast<uint8_t *>(&advTxPowerLevels[sizeof(PowerLevels_t)-1]),
        sizeof(uint8_t)
    );
}

/* WRITE AUTHORIZATION */

void EddystoneService::writeUnlockAuthorizationCallback(GattWriteAuthCallbackParams *authParams)
{
    if (lockState == UNLOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
    } else if (authParams->len != sizeof(Lock_t)) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
    } else if (authParams->offset != 0) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
    } else if (memcmp(authParams->data, unlockToken, sizeof(Lock_t)) != 0) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
    } else {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }
}

void EddystoneService::writeVarLengthDataAuthorizationCallback(GattWriteAuthCallbackParams *authParams)
{
   if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
    } else if (authParams->len > 19) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
    } else if (authParams->offset != 0) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
    } else {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }
}

void EddystoneService::writeLockStateAuthorizationCallback(GattWriteAuthCallbackParams *authParams)
{
   if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
    } else if ((authParams->len != sizeof(uint8_t)) && (authParams->len != (sizeof(uint8_t) + sizeof(Lock_t)))) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
    } else if (authParams->offset != 0) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
    } else {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }
}

template <typename T>
void EddystoneService::writeBasicAuthorizationCallback(GattWriteAuthCallbackParams *authParams)
{
    if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
    } else if (authParams->len != sizeof(T)) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
    } else if (authParams->offset != 0) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
    } else {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }
}

template <typename T>
void EddystoneService::writeActiveSlotAuthorizationCallback(GattWriteAuthCallbackParams *authParams)
{
    if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_WRITE_NOT_PERMITTED;
    } else if (authParams->len != sizeof(T)) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
    } else if (*(authParams->data) > MAX_ADV_SLOTS -1) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_ATT_VAL_LENGTH;
    } else if (authParams->offset != 0) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_INVALID_OFFSET;
    } else {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }
}

/* READ AUTHORIZTION */

void EddystoneService::readBasicTestLockAuthorizationCallback(GattReadAuthCallbackParams *authParams)
{
    if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_READ_NOT_PERMITTED;
    } else {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
    }
}

void EddystoneService::readDataAuthorizationCallback(GattReadAuthCallbackParams *authParams)
{
    uint8_t frameType = slotFrameTypes[activeSlot];
    uint8_t* frame = slotToFrame(activeSlot);
    uint8_t slotLength = 1;
    uint8_t buf[14] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint8_t* slotData = buf;
    uint16_t advInterval = slotAdvIntervals[activeSlot];
    
    if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_READ_NOT_PERMITTED;
        return;
    }
   
    if (advInterval != 0) {
        switch(frameType) {
            case EDDYSTONE_FRAME_UID:
                slotLength = uidFrame.getDataLength(frame); 
                slotData = uidFrame.getData(frame);
                break;
            case EDDYSTONE_FRAME_URL:
                slotLength = urlFrame.getDataLength(frame); 
                slotData = urlFrame.getData(frame);
                break;
            case EDDYSTONE_FRAME_TLM:
                updateRawTLMFrame(frame);
                slotLength = tlmFrame.getDataLength(frame);
                slotData = tlmFrame.getData(frame);
                break;
            case EDDYSTONE_FRAME_EID:
                slotLength = 14;
                buf[0] = EIDFrame::FRAME_TYPE_EID;
                buf[1] = slotEidRotationPeriodExps[activeSlot];
                // Add time as a big endian 32 bit number
                uint32_t timeSecs = timeSinceBootTimer.read_ms() / 1000;
                buf[2] = timeSecs  >> 24;
                buf[3] = (timeSecs & 0xffffff) >> 16;
                buf[4] = (timeSecs & 0xffff) >> 8;
                buf[5] = timeSecs & 0xff;
                memcpy(buf + 6, eidFrame.getData(frame), 8);
                slotData = buf;
                break;
        }
    }
    
    ble.gattServer().write(advSlotDataChar->getValueHandle(), slotData, slotLength);
    authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
}

void EddystoneService::readUnlockAuthorizationCallback(GattReadAuthCallbackParams *authParams)
{
    if (lockState == UNLOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_READ_NOT_PERMITTED;
        return;
    }
    generateRandom(challenge, sizeof(Lock_t));
    aes128Encrypt(unlockKey, challenge, unlockToken);
    ble.gattServer().write(unlockChar->getValueHandle(), reinterpret_cast<uint8_t *>(challenge), sizeof(Lock_t));      // Update the challenge
    authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
}

void EddystoneService::readAdvIntervalAuthorizationCallback(GattReadAuthCallbackParams *authParams)
{
    if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_READ_NOT_PERMITTED;
        return;
    }
    uint16_t beAdvInterval = swapEndian(slotAdvIntervals[activeSlot]);
    ble.gattServer().write(advIntervalChar->getValueHandle(), reinterpret_cast<uint8_t *>(&beAdvInterval), sizeof(uint16_t));
    authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
}

void EddystoneService::readRadioTxPowerAuthorizationCallback(GattReadAuthCallbackParams *authParams)
{
    if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_READ_NOT_PERMITTED;
        return;
    }
    int8_t radioTxPower = slotRadioTxPowerLevels[activeSlot];
    ble.gattServer().write(radioTxPowerChar->getValueHandle(), reinterpret_cast<uint8_t *>(&radioTxPower), sizeof(int8_t));
    authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
}

void EddystoneService::readAdvTxPowerAuthorizationCallback(GattReadAuthCallbackParams *authParams)
{
    if (lockState == LOCKED) {
        authParams->authorizationReply = AUTH_CALLBACK_REPLY_ATTERR_READ_NOT_PERMITTED;
        return;
    }
    int8_t advTxPower = slotAdvTxPowerLevels[activeSlot];
    ble.gattServer().write(advTxPowerChar->getValueHandle(), reinterpret_cast<uint8_t *>(&advTxPower), sizeof(int8_t));
    authParams->authorizationReply = AUTH_CALLBACK_REPLY_SUCCESS;
}

/*
 * This callback is invoked when a GATT client attempts to modify any of the
 * characteristics of this service. Attempts to do so are also applied to
 * the internal state of this service object.
 */
void EddystoneService::onDataWrittenCallback(const GattWriteCallbackParams *writeParams)
{
    uint16_t handle = writeParams->handle;
    // CHAR-1 CAPABILITIES
            /* capabilitySlotChar is READ ONLY */
    // CHAR-2 ACTIVE SLOT
    if (handle == activeSlotChar->getValueHandle()) {
        uint8_t slot = *(writeParams->data);
        // Ensure slot does not exceed limit, or set highest slot
        if (slot < MAX_ADV_SLOTS) {
            activeSlot = slot;
        }
        ble.gattServer().write(activeSlotChar->getValueHandle(), &activeSlot, sizeof(uint8_t)); 
    // CHAR-3 ADV INTERVAL
    } else if (handle == advIntervalChar->getValueHandle()) {
        uint16_t interval = correctAdvertisementPeriod(swapEndian(*((uint16_t *)(writeParams->data)))); 
        slotAdvIntervals[activeSlot] = interval; // Store this value for reading
        uint16_t beAdvInterval = swapEndian(slotAdvIntervals[activeSlot]);
        ble.gattServer().write(advIntervalChar->getValueHandle(), reinterpret_cast<uint8_t *>(&beAdvInterval), sizeof(uint16_t));
    // CHAR-4 RADIO TX POWER
    } else if (handle == radioTxPowerChar->getValueHandle()) {
        int8_t radioTxPower = *(writeParams->data);
        uint8_t index = radioTxPowerToIndex(radioTxPower);
        radioTxPower = radioTxPowerLevels[index]; // Power now corrected to nearest allowed power
        slotRadioTxPowerLevels[activeSlot] = radioTxPower; // Store by slot number
        int8_t advTxPower = advTxPowerLevels[index]; // Determine adv power equivalent
        slotAdvTxPowerLevels[activeSlot] = advTxPower;
        setFrameTxPower(activeSlot, advTxPower); // Set the actual frame radio TxPower for this slot
        ble.gattServer().write(radioTxPowerChar->getValueHandle(), reinterpret_cast<uint8_t *>(&radioTxPower), sizeof(int8_t));
    // CHAR-5 ADV TX POWER
    } else if (handle == advTxPowerChar->getValueHandle()) {
        int8_t advTxPower = *(writeParams->data);
        slotAdvTxPowerLevels[activeSlot] = advTxPower;
        setFrameTxPower(activeSlot, advTxPower); // Update the actual frame Adv TxPower for this slot
        ble.gattServer().write(advTxPowerChar->getValueHandle(), reinterpret_cast<uint8_t *>(&advTxPower), sizeof(int8_t));
    // CHAR-6 LOCK STATE
    } else if (handle == lockStateChar->getValueHandle()) {
        uint8_t newLockState = *(writeParams->data);
        if ((writeParams->len == sizeof(uint8_t)) || (writeParams->len == sizeof(uint8_t) + sizeof(Lock_t))) {
            if ((newLockState == LOCKED) || (newLockState == UNLOCKED) || (newLockState == UNLOCKED_AUTO_RELOCK_DISABLED)) {
                lockState = newLockState;
            } 
        }
        if ((newLockState == LOCKED) && (writeParams->len == (sizeof(uint8_t) + sizeof(Lock_t))) ) {
            // And sets the new secret lock code if present
            uint8_t encryptedNewKey[sizeof(Lock_t)];
            uint8_t newKey[sizeof(Lock_t)];
            memcpy(encryptedNewKey, (writeParams->data)+1, sizeof(Lock_t));
            // Decrypt the new key
            aes128Decrypt(unlockKey, encryptedNewKey, newKey);
            memcpy(unlockKey, newKey, sizeof(Lock_t));
        } 
        ble.gattServer().write(lockStateChar->getValueHandle(), reinterpret_cast<uint8_t *>(&lockState), sizeof(uint8_t));
    // CHAR-7 UNLOCK
    } else if (handle == unlockChar->getValueHandle()) {
       // NOTE: Actual comparison with unlock code is done in:
       // writeUnlockAuthorizationCallback(...)  which is executed before this method call.
       lockState = UNLOCKED;
       // Regenerate challenge and expected unlockToken for Next unlock operation
       generateRandom(challenge, sizeof(Lock_t));
       aes128Encrypt(unlockKey, challenge, unlockToken);
       // Update Chars
       ble.gattServer().write(unlockChar->getValueHandle(), reinterpret_cast<uint8_t *>(challenge), sizeof(Lock_t));      // Update the challenge
       ble.gattServer().write(lockStateChar->getValueHandle(), reinterpret_cast<uint8_t *>(&lockState), sizeof(uint8_t)); // Update the lock
    // CHAR-8 PUBLIC ECDH KEY
        /* PublicEchdChar is READ ONLY */
    // CHAR-9 EID INDENTITY KEY
        /* EidIdentityChar is READ ONLY */
    // CHAR-10 ADV DATA
    } else if (handle == advSlotDataChar->getValueHandle()) { 
        uint8_t* frame = slotToFrame(activeSlot);
        int8_t advTxPower = slotAdvTxPowerLevels[activeSlot];
        uint8_t writeFrameFormat = *(writeParams->data);
        uint8_t writeFrameLen = (writeParams->len) - 1;
        uint8_t writeData[34];
        uint8_t serverPublicEcdhKey[32];
        memcpy(writeData, (writeParams->data) + 1, writeFrameLen);
        
        switch(writeFrameFormat) {
            case UIDFrame::FRAME_TYPE_UID: 
                if (writeFrameLen == 16) {
                    uidFrame.setData(frame, advTxPower,reinterpret_cast<const uint8_t *>((writeParams->data) + 1)); 
                    slotFrameTypes[activeSlot] = EDDYSTONE_FRAME_UID;
                }
                break;
            case URLFrame::FRAME_TYPE_URL: 
               if (writeFrameLen <= 18) {
                    urlFrame.setData(frame, advTxPower, reinterpret_cast<const uint8_t*>((writeParams->data) + 1), writeFrameLen ); 
                    slotFrameTypes[activeSlot] = EDDYSTONE_FRAME_URL;
                }
                break;
            case TLMFrame::FRAME_TYPE_TLM:
                if (writeFrameLen == 0) {
                    updateRawTLMFrame(frame);
                    tlmFrame.setData(frame);
                    slotFrameTypes[activeSlot] = EDDYSTONE_FRAME_TLM;
                }
                break; 
            case EIDFrame::FRAME_TYPE_EID: 
                if (writeFrameLen == 17) {
                    // Least secure
                    aes128Decrypt(unlockKey, writeData, slotEidIdentityKeys[activeSlot]);
                    slotEidRotationPeriodExps[activeSlot] = writeData[16]; // index 16 is the exponent
                } else if (writeFrameLen == 33) {
                    // Most secure
                    memcpy(serverPublicEcdhKey, writeData, 32);
                    slotEidRotationPeriodExps[activeSlot] = writeData[32]; // index 32 is the exponent
                    eidFrame.genEcdhSharedKey(privateEcdhKey, publicEcdhKey, serverPublicEcdhKey, slotEidIdentityKeys[activeSlot]);
                } else {
                    break; // Do nothing, this is not a recognized Frame length
                }
                // Establish the new frame type
                slotFrameTypes[activeSlot] = EDDYSTONE_FRAME_EID;
                // Generate ADV frame packet from EidIdentity Key
                eidFrame.update(frame, slotEidIdentityKeys[activeSlot], slotEidRotationPeriodExps[activeSlot], timeSinceBootTimer.read_ms() / 1000); 
                break;
            default: 
                // Do nothing, this is not a recognized Frame format
                break;
        }
        // Read takes care of setting the Characteristic  Value
    // CHAR-11 FACTORY RESET
    } else if (handle == factoryResetChar->getValueHandle() && (*((uint8_t *)writeParams->data) != 0)) {
        // Reset parmas to default values 
        doFactoryReset();
        // Update all characteristics based on params
        updateCharacteristicValues();
    // CHAR-12 REMAIN CONNECTABLE
    } else if (handle == remainConnectableChar->getValueHandle()) {
        remainConnectable = *(writeParams->data); 
        ble.gattServer().write(remainConnectableChar->getValueHandle(), &remainConnectable, sizeof(uint8_t)); 
    }
    
}

void EddystoneService::setFrameTxPower(uint8_t slot, int8_t advTxPower) {
    uint8_t* frame = slotToFrame(slot);
    uint8_t frameType = slotFrameTypes[slot];
    switch (frameType) { 
        case UIDFrame::FRAME_TYPE_UID: 
           uidFrame.setAdvTxPower(frame, advTxPower);
           break;
        case URLFrame::FRAME_TYPE_URL: 
           urlFrame.setAdvTxPower(frame, advTxPower);
           break;
        case EIDFrame::FRAME_TYPE_EID: 
           eidFrame.setAdvTxPower(frame, advTxPower);
           break;
    }
}

uint8_t EddystoneService::radioTxPowerToIndex(int8_t txPower) {
    // NOTE: txPower is an 8-bit signed number
    uint8_t size = sizeof(PowerLevels_t);
    // Look for the value in range (or next biggest value)
    for (uint8_t i = 0; i < size; i++) {
      if (txPower <= radioTxPowerLevels[i]) {
          return i;
      }
    }
    return size - 1;
}

/** AES128 encrypts a 16-byte input array with a key, resulting in a 16-byte output array */
void EddystoneService::aes128Encrypt(uint8_t key[], uint8_t input[], uint8_t output[]) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx); 
    mbedtls_aes_setkey_enc(&ctx, key, 8 * sizeof(Lock_t));
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, input, output);
    mbedtls_aes_free(&ctx);
}

/** AES128 decrypts a 16-byte input array with a key, resulting in a 16-byte output array */
void EddystoneService::aes128Decrypt(uint8_t key[], uint8_t input[], uint8_t output[]) {
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx); 
    mbedtls_aes_setkey_dec(&ctx, key, 8 * sizeof(Lock_t));
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, input, output);
    mbedtls_aes_free(&ctx);
}

/** Generates a set of random values in byte array[size]  */
void EddystoneService::generateRandom(uint8_t ain[], int size) {
    int i;
    // Random seed based on boot time in milliseconds
    srand(timeSinceBootTimer.read_ms()); 
    for (i = 0; i < size; i++) {
        ain[i] = rand() % 256;
    }
    return;
}

/*  ALTERNATE Better Random number generator (but has Memory usage issues)
Generates a set of random values in byte array[size] 
void EddystoneService::generateRandom(uint8_t ain[], int size) {
    mbedtls_entropy_context entropy;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    mbedtls_ctr_drbg_random(&ctr_drbg, ain, size);
    mbedtls_entropy_free(&entropy);
    return;
}
*/

/** Reverse Array endianess: Big to Little or Little to Big */
void EddystoneService::swapEndianArray(uint8_t ptrIn[], uint8_t ptrOut[], int size) {
    int i;
    for (i = 0; i < size; i++) {
        ptrIn[i] = ptrOut[size - i - 1];
    }
    return;
}

/** Reverse endianess: Big to Little or Little to Big */
uint16_t EddystoneService::swapEndian(uint16_t arg) {
    return (arg / 256) + (arg % 256) * 256;
}

uint16_t EddystoneService::correctAdvertisementPeriod(uint16_t beaconPeriodIn) const
{
    /* Re-map beaconPeriod to within permissible bounds if necessary. */
    if (beaconPeriodIn != 0) {
        if (beaconPeriodIn < ble.gap().getMinNonConnectableAdvertisingInterval()) {
            return ble.gap().getMinNonConnectableAdvertisingInterval();
        } else if (beaconPeriodIn > ble.gap().getMaxAdvertisingInterval()) {
            return ble.gap().getMaxAdvertisingInterval();
        }
    }
    return beaconPeriodIn;
}
