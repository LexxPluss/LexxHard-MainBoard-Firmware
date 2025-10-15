/*
 * Copyright (c) 2025, LexxPluss Inc.
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

#include <sys/atomic.h>
#include "rosserial_board_store.hpp"

namespace {
class ros_board_store_impl {
public:
  void set_emergency_stop(bool v)   { ros2board.emergency_stop = v; }
  void set_power_off(bool v)        { ros2board.power_off = v; }
  void set_lockdown(bool v)         { ros2board.lockdown = v; }
  void set_wheel_power_off(bool v)  { ros2board.wheel_power_off = v; }
  void set_auto_charge_request_enable(bool v) {
    atomic_set(&pending_auto_charge, v ? 1 : 0);
  }
  lexxhard::can_controller::msg_control get_ros2board() {
    ros2board.auto_charge_request_enable = (atomic_get(&pending_auto_charge) != 0);
    return ros2board;
  }
private:
  atomic_t pending_auto_charge = ATOMIC_INIT(0);
  lexxhard::can_controller::msg_control ros2board{0};
} impl;
} // namespace

void lexxhard::ros_board_store::set_emergency_stop(bool data) {
  impl.set_emergency_stop(data);
}

void lexxhard::ros_board_store::set_power_off(bool data) {
  impl.set_power_off(data);
}

void lexxhard::ros_board_store::set_lockdown(bool data) {
  impl.set_lockdown(data);
}

void lexxhard::ros_board_store::set_wheel_power_off(bool data) {
  impl.set_wheel_power_off(data);
}

void lexxhard::ros_board_store::set_auto_charge_request_enable(bool data) {
  impl.set_auto_charge_request_enable(data);
}

lexxhard::can_controller::msg_control lexxhard::ros_board_store::get_ros2board() {
  return impl.get_ros2board();
}
