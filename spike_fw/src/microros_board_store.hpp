/*
 * Board request store of the micro-ROS subscribers (unit h). One to one port of rosserial_board_store.hpp.
 */
#pragma once
#include "can_controller.hpp"

namespace lexxhard::ros_board_store {
void set_emergency_stop(bool data);
void set_power_off(bool data);
void set_lockdown(bool data);
void set_wheel_power_off(bool data);
void set_auto_charge_request_enable(bool data);
lexxhard::can_controller::msg_control get_ros2board();
}
