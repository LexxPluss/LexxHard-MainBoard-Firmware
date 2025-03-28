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

#include <shell/shell.h>

#include "rosserial_hardware_zephyr.hpp"
#include "rosserial_actuator.hpp"
#include "rosserial_bmu.hpp"
#include "rosserial_board.hpp"
#include "rosserial_dfu.hpp"
#include "rosserial_imu.hpp"
#include "rosserial_interlock.hpp"
#include "rosserial_led.hpp"
#include "rosserial_pgv.hpp"
#include "rosserial_tof.hpp"
#include "rosserial_uss.hpp"
#include "rosserial.hpp"
#include "rosserial_towing_unit.hpp"

namespace lexxhard::rosserial {

class {
public:
    int init() {
        nh.getHardware()->set_baudrate(921600);
        nh.initNode(const_cast<char*>("UART_6"));
        actuator.init(nh);
        bmu.init(nh);
        board.init(nh);
        dfu.init(nh);
        imu.init(nh);
        interlock.init(nh);
        led.init(nh);
        pgv.init(nh);
        tof.init(nh);
        uss.init(nh);
        towing_unit.init(nh);
        return 0;
    }
    void run() {
        init_profile();

        while (true) {
            nh.spinOnce();
            actuator.poll();
            bmu.poll();
            board.poll();
            dfu.poll();
            imu.poll();
            interlock.poll();
            led.poll();
            pgv.poll();
            tof.poll();
            uss.poll();
            towing_unit.poll();

            update_profile();
            k_usleep(1);
        }
    }

    void show_profile(const shell *shell) {
        shell_fprintf(shell, SHELL_NORMAL, "TX bytes per sec: %.2f\n", profile.tx_bytes_per_sec);
        shell_fprintf(shell, SHELL_NORMAL, "RX bytes per sec: %.2f\n", profile.rx_bytes_per_sec);
        shell_fprintf(shell, SHELL_NORMAL, "Max TX bytes per sec: %.2f\n", profile.max_tx_bytes_per_sec);
        shell_fprintf(shell, SHELL_NORMAL, "Max RX bytes per sec: %.2f\n", profile.max_rx_bytes_per_sec);
        shell_fprintf(shell, SHELL_NORMAL, "TX drop bytes: %u\n", nh.getHardware()->get_transport_profile().drop_tx_bytes);
        shell_fprintf(shell, SHELL_NORMAL, "RX drop bytes: %u\n", nh.getHardware()->get_transport_profile().drop_rx_bytes);
    }
private:
    ros::NodeHandle nh;
    ros_actuator actuator;
    ros_bmu bmu;
    ros_board board;
    ros_dfu dfu;
    ros_imu imu;
    ros_interlock interlock;
    ros_led led;
    ros_pgv pgv;
    ros_tof tof;
    ros_uss uss;
    ros_towing_unit towing_unit;

    void init_profile() {
        auto const cur_profile{nh.getHardware()->get_transport_profile()};
        profile.last_time = cur_profile.time;
        profile.last_tx_bytes = cur_profile.tx_bytes;
        profile.last_rx_bytes = cur_profile.rx_bytes;
        profile.last_drop_tx_bytes = cur_profile.drop_tx_bytes;
        profile.last_drop_rx_bytes = cur_profile.drop_rx_bytes;
    }

    void update_profile() {
        auto const dt{nh.getHardware()->time() - profile.last_time};
        if (dt < 1000) {
            return;
        }

        auto const cur_profile{nh.getHardware()->get_transport_profile()};
        auto const cur_dt{(cur_profile.time - profile.last_time) / 1000.0f};
        profile.tx_bytes_per_sec = (cur_profile.tx_bytes - profile.last_tx_bytes) / cur_dt;
        profile.rx_bytes_per_sec = (cur_profile.rx_bytes - profile.last_rx_bytes) / cur_dt;
        profile.max_tx_bytes_per_sec = std::max(profile.max_tx_bytes_per_sec, profile.tx_bytes_per_sec);
        profile.max_rx_bytes_per_sec = std::max(profile.max_rx_bytes_per_sec, profile.rx_bytes_per_sec);

        profile.last_time = cur_profile.time;
        profile.last_tx_bytes = cur_profile.tx_bytes;
        profile.last_rx_bytes = cur_profile.rx_bytes;
        profile.last_drop_tx_bytes = cur_profile.drop_tx_bytes;
        profile.last_drop_rx_bytes = cur_profile.drop_rx_bytes;
    }

    // for profile
    struct {
        uint32_t last_time{0};
        uint32_t last_tx_bytes{0};
        uint32_t last_rx_bytes{0};
        uint32_t last_drop_tx_bytes{0};
        uint32_t last_drop_rx_bytes{0};
        float tx_bytes_per_sec{0};
        float rx_bytes_per_sec{0};
        float max_tx_bytes_per_sec{0};
        float max_rx_bytes_per_sec{0};
    } profile;
} impl;


int profile(const shell *shell, size_t argc, char **argv)
{
    impl.show_profile(shell);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ros,
    SHELL_CMD(profile, NULL, "ROS serial profile", profile),
    SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(ros, &sub_ros, "ROS serial commands", NULL);

void init()
{
    impl.init();
}

void run(void *p1, void *p2, void *p3)
{
    impl.run();
}

k_thread thread;

}

// vim: set expandtab shiftwidth=4:
