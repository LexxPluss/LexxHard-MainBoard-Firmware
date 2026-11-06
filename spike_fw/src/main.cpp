/*
 * Copyright (c) 2022, LexxPluss Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#if FW_HAS_ACTUATOR
#include "actuator_controller.hpp"
#endif
#if FW_HAS_ADC
#include "adc_reader.hpp"
#endif
#if FW_HAS_CAN
#include "can_controller.hpp"
#endif
#if FW_HAS_UPDATER
#include "firmware_updater.hpp"
#endif
#if FW_HAS_IMU
#include "imu_controller.hpp"
#endif
#if FW_HAS_INTERLOCK
#include "interlock_controller.hpp"
#endif
#if FW_HAS_LED
#include "led_controller.hpp"
#endif
#if FW_HAS_MISC
#include "misc_controller.hpp"
#endif
#if FW_HAS_PGV
#include "pgv_controller.hpp"
#endif
#if FW_HAS_RUNAWAY
#include "runaway_detector.hpp"
#endif
#if FW_HAS_TOF
#include "tof_controller.hpp"
#endif
#if FW_HAS_USS
#include "uss_controller.hpp"
#endif
#if FW_HAS_TOWING
#include "towing_unit_controller.hpp"
#endif
#if defined(FW_HAS_MICROROS_REAL)
#include "microros_session.h"
#elif FW_HAS_MICROROS
#include "microros_types.h"
#endif

namespace {

#if FW_HAS_ACTUATOR
K_THREAD_STACK_DEFINE(actuator_controller_stack, 2048);
#endif
#if FW_HAS_ADC
K_THREAD_STACK_DEFINE(adc_reader_stack, 2048);
#endif
#if FW_HAS_CAN
K_THREAD_STACK_DEFINE(can_controller_stack, 2048);
#endif
#if FW_HAS_UPDATER
K_THREAD_STACK_DEFINE(firmware_updater_stack, 2048);
#endif
#if FW_HAS_IMU
K_THREAD_STACK_DEFINE(imu_controller_stack, 2048);
#endif
#if FW_HAS_INTERLOCK
K_THREAD_STACK_DEFINE(interlock_controller_stack, 2048);
#endif
#if FW_HAS_LED
K_THREAD_STACK_DEFINE(led_controller_stack, 2048);
#endif
#if FW_HAS_MISC
K_THREAD_STACK_DEFINE(misc_controller_stack, 2048);
#endif
#if FW_HAS_PGV
K_THREAD_STACK_DEFINE(pgv_controller_stack, 2048);
#endif
#if FW_HAS_RUNAWAY
K_THREAD_STACK_DEFINE(runaway_detector_stack, 2048);
#endif
#if FW_HAS_TOF
K_THREAD_STACK_DEFINE(tof_controller_stack, 2048);
#endif
#if FW_HAS_USS
K_THREAD_STACK_DEFINE(uss_controller_stack, 2048);
#endif
#if FW_HAS_TOWING
K_THREAD_STACK_DEFINE(towing_unit_controller_stack, 2048);
#endif
#if defined(FW_HAS_MICROROS_REAL)
K_THREAD_STACK_DEFINE(microros_uart6_stack, 25000);
struct k_thread microros_uart6_thread;
K_THREAD_STACK_DEFINE(microros_uart2_stack, 25000);
struct k_thread microros_uart2_thread;
#elif FW_HAS_MICROROS
K_THREAD_STACK_DEFINE(microros_stack, 25000);
struct k_thread microros_thread;
#endif

#define RUN(name, prio) \
    k_thread_create(&lexxhard::name::thread, name##_stack, K_THREAD_STACK_SIZEOF(name##_stack), \
                    lexxhard::name::run, nullptr, nullptr, nullptr, prio, K_FP_REGS, K_MSEC(2000));

void reset_usb_hub()
{
    if (const device *gpioj{device_get_binding("GPIOJ")}; device_is_ready(gpioj)) {
        gpio_pin_configure(gpioj, 13, GPIO_OUTPUT_HIGH | GPIO_ACTIVE_HIGH);
        k_msleep(1);
        gpio_pin_set(gpioj, 13, 0);
        k_msleep(1);
        gpio_pin_set(gpioj, 13, 1);
    }
}

uint8_t get_board_setting()
{
    uint8_t brd_setting{0}, temp{0};

    const device *gpiof{device_get_binding("GPIOF")};

    if (device_is_ready(gpiof)) {
        gpio_pin_configure(gpiof, 12, GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_HIGH);   //SPRGPI09
        gpio_pin_configure(gpiof, 13, GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_HIGH);   //SPRGPI10

        brd_setting = uint8_t(gpio_pin_get(gpiof, 12));
        temp = uint8_t(gpio_pin_get(gpiof, 13));
        temp = temp << 1;
        brd_setting = brd_setting | temp;
    }

    return brd_setting;
}

}

int main()
{
    reset_usb_hub();
#if FW_HAS_ACTUATOR
    lexxhard::actuator_controller::init();
#endif
#if FW_HAS_ADC
    lexxhard::adc_reader::init();
#endif
#if FW_HAS_CAN
    lexxhard::can_controller::init();
#endif
#if FW_HAS_UPDATER
    lexxhard::firmware_updater::init();
#endif
#if FW_HAS_IMU
    lexxhard::imu_controller::init();
#endif
#if FW_HAS_INTERLOCK
    lexxhard::interlock_controller::init();
#endif
#if FW_HAS_LED
    lexxhard::led_controller::init();
#endif
#if FW_HAS_MISC
    lexxhard::misc_controller::init();
#endif
#if FW_HAS_PGV
    lexxhard::pgv_controller::init();
#endif
#if FW_HAS_RUNAWAY
    lexxhard::runaway_detector::init();
#endif
#if FW_HAS_TOF
    lexxhard::tof_controller::init();
#endif
#if FW_HAS_USS
    lexxhard::uss_controller::init();
#endif
    
#if FW_HAS_ACTUATOR
    RUN(actuator_controller, 2);
#endif
#if FW_HAS_ADC
    RUN(adc_reader, 2);
#endif
#if FW_HAS_CAN
    RUN(can_controller, 4);
#endif
#if FW_HAS_UPDATER
    RUN(firmware_updater, 7);
#endif
#if FW_HAS_IMU
    RUN(imu_controller, 2);
#endif
#if FW_HAS_INTERLOCK
    RUN(interlock_controller, 5);
#endif
#if FW_HAS_LED
    RUN(led_controller, 1);
#endif
#if FW_HAS_MISC
    RUN(misc_controller, 2);
#endif
#if FW_HAS_PGV
    RUN(pgv_controller, 1);
#endif
#if FW_HAS_TOF
    RUN(tof_controller, 2);
#endif
#if FW_HAS_USS
    RUN(uss_controller, 2);
#endif
#if FW_HAS_RUNAWAY
    RUN(runaway_detector, 4);
#endif

#if defined(FW_HAS_MICROROS_REAL)
    k_thread_create(&microros_uart6_thread, microros_uart6_stack, K_THREAD_STACK_SIZEOF(microros_uart6_stack),
                    [](void *, void *, void *) { lexxhard_microros_session_uart6(); },
                    nullptr, nullptr, nullptr, 5, K_FP_REGS, K_MSEC(2000));
    k_thread_create(&microros_uart2_thread, microros_uart2_stack, K_THREAD_STACK_SIZEOF(microros_uart2_stack),
                    [](void *, void *, void *) { lexxhard_microros_session_uart2(); },
                    nullptr, nullptr, nullptr, 6, K_FP_REGS, K_MSEC(2000));
#elif FW_HAS_MICROROS
    k_thread_create(&microros_thread, microros_stack, K_THREAD_STACK_SIZEOF(microros_stack),
                    [](void *, void *, void *) { lexxhard_microros_run(); },
                    nullptr, nullptr, nullptr, 6, K_FP_REGS, K_MSEC(2000));
#endif

    switch (get_board_setting()) {
        case 0: //Wani Unit
#if FW_HAS_TOWING
            lexxhard::towing_unit_controller::init();
            RUN(towing_unit_controller, 2);
#endif
            break;
        case 1: //Reserved
            break;
        case 2: //Reserved
            break;
        default:
            break;
    }

    const device *gpiog{device_get_binding("GPIOG")};
    if (gpiog != nullptr)
        gpio_pin_configure(gpiog, 7, GPIO_OUTPUT_LOW | GPIO_ACTIVE_HIGH);
    int heartbeat_led{1};
    while (true) {
        if (gpiog != nullptr) {
            gpio_pin_set(gpiog, 7, heartbeat_led);
            heartbeat_led = !heartbeat_led;
        }
        k_msleep(1000);
    }
}

// vim: set expandtab shiftwidth=4:
