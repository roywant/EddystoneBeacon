/*
 * PackageLicenseDeclared: Apache-2.0
 * Copyright (c) 2015 ARM Limited
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

/**
 * NOTE: THIS FILE IS A WORKAROUND FOR mbed::util::CriticalSectionLock implementation
 * it should be removed as soon as https://github.com/ARMmbed/core-util/pull/50 is
 * accepted and published.
 */

#ifndef BLE_CLIAPP_UTIL_NORDIC_CRITICAL_SECTION_LOCK_H__
#define BLE_CLIAPP_UTIL_NORDIC_CRITICAL_SECTION_LOCK_H__

#include <stdint.h>
#include "cmsis.h"
#include "nrf_soc.h"
#include "nrf_sdm.h"
#include "nrf_nvic.h"

namespace util {

/** RAII object for disabling, then restoring, interrupt state
  * Usage:
  * @code
  *
  * void f() {
  *     // some code here
  *     {
  *         NordicCriticalSectionLock lock;
  *         // Code in this block will run with interrupts disabled
  *     }
  *     // interrupts will be restored to their previous state
  * }
  * @endcode
  */
class NordicCriticalSectionLock {
public:

    /**
     * Start a critical section, the critical section will end when the
     * destructor is invoked.
     */
    NordicCriticalSectionLock() {
        // get the state of exceptions for the CPU
        _PRIMASK_state = __get_PRIMASK();

        // if exceptions are not enabled, there is nothing more to do
        if(_PRIMASK_state == 1) {
          _use_softdevice_routine = false;
        } else {
          // otherwise, use soft device routine if softdevice is running or disable
          // the irq if softdevice is not running
          uint8_t sd_enabled;
          if((sd_softdevice_is_enabled(&sd_enabled) == NRF_SUCCESS) && sd_enabled == 1) {
            _use_softdevice_routine = true;
            sd_nvic_critical_region_enter(&_sd_state);
          } else {
            _use_softdevice_routine = false;
            __disable_irq();
          }
        }
    }

    /**
     * eqnd a critical section
     */
    ~NordicCriticalSectionLock() {
        if(_use_softdevice_routine) {
          sd_nvic_critical_region_exit(_sd_state);
        } else {
          __set_PRIMASK(_PRIMASK_state);
        }
    }

private:
    union {
      uint32_t _PRIMASK_state;
      uint8_t  _sd_state;
    };
    bool _use_softdevice_routine;
};

} // namespace util

#endif // #ifndef BLE_CLIAPP_UTIL_NORDIC_CRITICAL_SECTION_LOCK_H__
