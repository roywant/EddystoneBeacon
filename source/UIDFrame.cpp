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

#include "UIDFrame.h"

UIDFrame::UIDFrame(void)
{
}

void UIDFrame::setData(uint8_t *rawFrame, int8_t advTxPower, const uint8_t* uidData)
{
    size_t index = 0;
    rawFrame[index++] = UID_LENGTH + 4;                         // UID length + overhead of four bytes below
    rawFrame[index++] = EDDYSTONE_UUID[0];                      // 16-bit Eddystone UUID
    rawFrame[index++] = EDDYSTONE_UUID[1];
    rawFrame[index++] = FRAME_TYPE_UID;                         // 1B  Type
    rawFrame[index++] = advTxPower;                             // 1B  Power @ 0meter

    memcpy(rawFrame + index, uidData, UID_LENGTH);              // UID = 10B NamespaceID + 6B InstanceID
}

uint8_t* UIDFrame::getData(uint8_t* rawFrame)
{
        return &(rawFrame[3]);
}

uint8_t  UIDFrame::getDataLength(uint8_t* rawFrame)
{
     return rawFrame[0] - 2;
}

uint8_t* UIDFrame::getAdvFrame(uint8_t* rawFrame)
{
    return &(rawFrame[1]);
}

uint8_t UIDFrame::getAdvFrameLength(uint8_t* rawFrame)
{
    return rawFrame[0];
}

uint8_t* UIDFrame::getUid(uint8_t* rawFrame)
{
    return &(rawFrame[5]);
}

uint8_t UIDFrame::getUidLength(uint8_t* rawFrame)
{
    return rawFrame[0] - 4;
}

void UIDFrame::setAdvTxPower(uint8_t* rawFrame, int8_t advTxPower)
{
    rawFrame[4] = advTxPower;
}