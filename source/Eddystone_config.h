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

#define EDDYSTONE_CFG_DEFAULT_DEVICE_NAME "ES G-EID"

#define EDDYSTONE_DEFAULT_CONFIG_ADV_INTERVAL 1000

#define EDDYSTONE_DEFAULT_CONFIG_ADVERTISEMENT_TIMEOUT_SECONDS 30

#define EDDYSTONE_DEFAULT_UNLOCK_KEY { \
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF \
}

#define EDDYSTONE_DEFAULT_RADIO_TX_POWER_LEVELS { -30, -16, -4, 4 }

#define EDDYSTONE_DEFAULT_ADV_TX_POWER_LEVELS { -42, -30, -25, -13 }

#define EDDYSTONE_DEFAULT_MAX_ADV_SLOTS 3

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

#define EDDYSTONE_DEFAULT_SLOT_EID_ROTATION_PERIOD_EXPS { 3, 10, 10 }

#define EDDYSTONE_DEFAULT_SLOT_TYPES { \
    EDDYSTONE_FRAME_EID, \
    EDDYSTONE_FRAME_UID, \
    EDDYSTONE_FRAME_URL \
}

#define EDDYSTONE_DEFAULT_SLOT_INTERVALS { 700, 0, 0 }

#define EDDYSTONE_DEFAULT_SLOT_TX_POWERS { 4, -4, 4 }

#endif /* EDDYSTONE_CONFIG_H_ */
