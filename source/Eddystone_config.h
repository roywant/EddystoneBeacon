/*
 * Copyright (c) 2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EDDYSTONE_CONFIG_H_
#define EDDYSTONE_CONFIG_H_

/**
 * Platform Target (if not set, default is nRF51-DK or nRF51-dongle)
 * NOTE: All targets are assumed to be 32K (in target.json) and S110 (in config.h)
 */
// #define MinewTech

// Version printed out on virtual terminal (independent of logging flag below)
#define BUILD_VERSION_STR "EID Version 1.00 2016-10-19:15:30\r\n"

/** 
 * DEBUG OPTIONS
 * For production: all defines below should be UNCOMMENTED
 */ 
#define GEN_BEACON_KEYS_AT_INIT
#define HARDWARE_RANDOM_NUM_GENERATOR
// #define EID_RANDOM_MAC
// #define INCLUDE_CONFIG_URL
// #define DONT_REMAIN_CONNECTABLE
// #define NO_4SEC_START_DELAY
// #define NO_LOGGING

/* Default enable printf logging, unless explicitly NO_LOGGING */
#ifdef NO_LOGGING
  #define LOG_PRINT 0
#else
  #define LOG_PRINT 1
#endif

#define LOG(x) do { if (LOG_PRINT) printf x; } while (0)

/**
 * NOTE1: If you don't define RESET_BUTTON, it won't compile the button handler in main.cpp
 * This also doesn't declare or use the SHUTDOWN_LED
 */
#ifdef MinewTech
  // *** MinewTech PIN defines *** 
  #define LED_OFF 0
  #define CONFIG_LED p15
  #define SHUTDOWN_LED p16
  #define RESET_BUTTON p18
#else
  // *** NRF51-DK or USB Dongle PIN defines ***
  #define LED_OFF 1
  #define CONFIG_LED LED3
#endif

/*
 * BEACON BEHAVIOR DEFINED BELOW
 */

/**
 * Note: If the CONFIG_URL is enabled (DEFINE above)
 *    The size of the DEVICE_NAME + Encoded Length of the CONFIG_URL
 *    must be LESS THAN OR EQUAL to 19
 */
#define EDDYSTONE_CONFIG_URL "http://c.pw3b.org"
 
#define EDDYSTONE_CFG_DEFAULT_DEVICE_NAME "Edystn V3.0"

#define EDDYSTONE_DEFAULT_MAX_ADV_SLOTS 3

#define EDDYSTONE_DEFAULT_CONFIG_ADV_INTERVAL 1000

#define EDDYSTONE_DEFAULT_CONFIG_ADVERTISEMENT_TIMEOUT_SECONDS 30

#define EDDYSTONE_DEFAULT_UNLOCK_KEY { \
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF \
}

#define EDDYSTONE_DEFAULT_RADIO_TX_POWER_LEVELS { -30, -16, -4, 4 }

#define EDDYSTONE_DEFAULT_ADV_TX_POWER_LEVELS { -42, -30, -25, -13 }

#define EDDYSTONE_DEFAULT_SLOT_URLS { \
    "http://cf.physical-web.org", \
    "http://www.mbed.com/", \
    "http://www.gap.com/" \
}

#define EDDYSTONE_DEFAULT_SLOT_UIDS { \
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F }, \
    { 0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0 }, \
    { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF } \
}

#define EDDYSTONE_DEFAULT_SLOT_EID_IDENTITY_KEYS { \
    { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF }, \
    { 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF }, \
    { 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF } \
}

#define EDDYSTONE_DEFAULT_SLOT_EID_ROTATION_PERIOD_EXPS { 4, 10, 10 }

#define EDDYSTONE_DEFAULT_SLOT_TYPES { \
    EDDYSTONE_FRAME_EID, \
    EDDYSTONE_FRAME_URL, \
    EDDYSTONE_FRAME_UID \
}

#define EDDYSTONE_DEFAULT_SLOT_INTERVALS { 500, 0, 0 }

#define EDDYSTONE_DEFAULT_SLOT_TX_POWERS { 4, 4, 4 }

/**
 * Lock constants
 */
#define LOCKED 0
#define UNLOCKED 1
#define UNLOCKED_AUTO_RELOCK_DISABLED 2

#define DEFAULT_LOCK_STATE UNLOCKED

/**
 * Set default number of adv slots
 */
const uint8_t MAX_ADV_SLOTS = EDDYSTONE_DEFAULT_MAX_ADV_SLOTS;

/**
 * Slot and Power and Interval Constants
 */
const uint8_t DEFAULT_SLOT = 0;

/**
 * Number of radio power modes supported
 */
const uint8_t NUM_POWER_MODES = 4;

/**
 * Default name for the BLE Device Name characteristic.
 */
const char DEFAULT_DEVICE_NAME[] = EDDYSTONE_CFG_DEFAULT_DEVICE_NAME;

/**
 * ES GATT Capability Constants (6 values)
 */
const uint8_t CAP_HDR_LEN = 6;  // The six constants below

const uint8_t ES_GATT_VERSION = 0;

const uint8_t MAX_EIDS = MAX_ADV_SLOTS;


const uint8_t CAPABILITIES = 0x03; // Per slot variable interval and variable Power

const uint8_t SUPPORTED_FRAMES_H = 0x00;

const uint8_t SUPPORTED_FRAMES_L = 0x0F;

/**
 * ES GATT Capability Constant Array storing the capability constants
 */
const uint8_t CAPABILITIES_DEFAULT[] = {ES_GATT_VERSION, MAX_ADV_SLOTS, MAX_EIDS, CAPABILITIES, \
                                        SUPPORTED_FRAMES_H, SUPPORTED_FRAMES_L};

#endif /* EDDYSTONE_CONFIG_H_ */
