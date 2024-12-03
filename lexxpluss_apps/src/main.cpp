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

#include <zephyr.h>
#include <drivers/gpio.h>
#include "can_controller.hpp"
#include "firmware_updater.hpp"
#include "imu_controller.hpp"
#include "misc_controller.hpp"
#include "pgv_controller.hpp"
#include "rosserial.hpp"

namespace {

K_THREAD_STACK_DEFINE(can_controller_stack, 2048);
K_THREAD_STACK_DEFINE(firmware_updater_stack, 2048);
K_THREAD_STACK_DEFINE(imu_controller_stack, 2048);
K_THREAD_STACK_DEFINE(misc_controller_stack, 2048);
K_THREAD_STACK_DEFINE(pgv_controller_stack, 2048);
K_THREAD_STACK_DEFINE(rosserial_stack, 2048);

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

}

void main()
{
    reset_usb_hub();
    lexxhard::can_controller::init();
    lexxhard::firmware_updater::init();
    lexxhard::imu_controller::init();
    lexxhard::misc_controller::init();
    lexxhard::pgv_controller::init();
    lexxhard::rosserial::init();
    
    RUN(can_controller, 4);
    RUN(firmware_updater, 7);
    RUN(imu_controller, 2);
    RUN(misc_controller, 2);
    RUN(pgv_controller, 1);

    RUN(rosserial, 5); // The rosserial thread will be started last.
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
