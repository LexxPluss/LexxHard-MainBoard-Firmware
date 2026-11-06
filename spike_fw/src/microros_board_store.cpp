/*
 * Board request store of the micro-ROS subscribers (unit h). One to one port of rosserial_board_store.cpp;
 * the old files are not compiled in unit h, so the names do not clash.
 */
#include <zephyr/sys/atomic.h>
#include "microros_board_store.hpp"

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
