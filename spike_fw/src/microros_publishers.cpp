/*
 * micro-ROS publishers of the UART6 session (unit h). One to one port of the publisher and poll parts of
 * rosserial_actuator, _bmu, _board, _dfu, _imu, _interlock, _pgv, _tof, _uss and _towing_unit.
 *
 * The publishers are created from the entity table microros_entities.h (kind PUB). Each poll function looks its
 * publisher up by topic name at compile time, so that a topic missing in the table fails the build.
 * No heap allocation: every sequence and string of the messages is backed by a static array.
 */
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <zephyr/kernel.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rmw_microros/rmw_microros.h>

#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/byte.h>
#include <std_msgs/msg/byte_multi_array.h>
#include <std_msgs/msg/float32.h>
#include <std_msgs/msg/float32_multi_array.h>
#include <std_msgs/msg/float64_multi_array.h>
#include <std_msgs/msg/int32_multi_array.h>
#include <std_msgs/msg/u_int16_multi_array.h>
#include <std_msgs/msg/u_int8.h>
#include <std_msgs/msg/u_int8_multi_array.h>
#include <sensor_msgs/msg/battery_state.h>
#include <sensor_msgs/msg/temperature.h>
#include <diagnostic_msgs/msg/diagnostic_array.h>
#include <diagnostic_msgs/msg/diagnostic_status.h>
#include <diagnostic_msgs/msg/key_value.h>
#include <lexxauto_msgs/msg/battery.h>
#include <lexxauto_msgs/msg/board_temperatures.h>
#include <lexxauto_msgs/msg/imu.h>
#include <lexxauto_msgs/msg/position_guide_vision.h>
#include "microros_typesupport.h"

#include "microros_publishers.h"

#include "actuator_controller.hpp"
#include "can_controller.hpp"
#include "common.hpp"
#include "firmware_updater.hpp"
#include "imu_controller.hpp"
#include "interlock_controller.hpp"
#include "pgv_controller.hpp"
#include "tof_controller.hpp"
#include "towing_unit_controller.hpp"
#include "uss_controller.hpp"

namespace lexxhard::microros_publishers {

namespace {

// ---- publisher table (generated from microros_entities.h) ----

#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) MICROROS_TOPIC_##kind(name)
#define MICROROS_TOPIC_PUB(name) name,
#define MICROROS_TOPIC_SUB(name)
#define MICROROS_TOPIC_SRV(name)
constexpr const char *const pub_topics[] = {
#include "microros_entities.h"
};
#undef MICROROS_ENTITY

constexpr std::size_t num_pub = sizeof pub_topics / sizeof pub_topics[0];

constexpr bool same_string(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

constexpr std::size_t pub_index(const char *topic)
{
    for (std::size_t i{0}; i < num_pub; ++i) {
        if (same_string(pub_topics[i], topic))
            return i;
    }
    return num_pub;
}

#define PUB_ID(ident, topic) \
    constexpr std::size_t ident = pub_index(topic); \
    static_assert(ident < num_pub, "topic is not a publisher in microros_entities.h: " topic)

PUB_ID(id_encoder, "/body_control/encoder_count");
PUB_ID(id_connection, "/body_control/shelf_connection");
PUB_ID(id_current, "/body_control/linear_actuator_current");
PUB_ID(id_battery, "/sensor_set/battery");
PUB_ID(id_imu, "/sensor_set/imu");
PUB_ID(id_emergency_connected_robot, "/control/emergency_stop_at_connected_robot");
PUB_ID(id_pgv, "/sensor_set/pgv");
PUB_ID(id_downward, "/sensor_set/downward");
PUB_ID(id_ultrasonic, "/sensor_set/ultrasonic");
PUB_ID(id_towing_unit, "/sensor_set/towing_unit");
PUB_ID(id_dfu_response, "/lexxhard/dfu_response");
PUB_ID(id_fan, "/sensor_set/fan");
PUB_ID(id_bumper, "/sensor_set/bumper");
PUB_ID(id_emergency_switch, "/sensor_set/emergency_switch");
PUB_ID(id_charge_status, "/body_control/charge_status");
PUB_ID(id_temperature, "/sensor_set/temperature");
PUB_ID(id_power_state, "/body_control/power_state");
PUB_ID(id_charge_delay, "/body_control/charge_heartbeat_delay");
PUB_ID(id_charge_voltage, "/body_control/charge_connector_voltage");
PUB_ID(id_diagnostics, "/diagnostics");

rcl_publisher_t publishers[num_pub];

// The old rosserial publish ignored failures; so does this.
void publish(std::size_t id, const void *message)
{
    const rcl_ret_t rc = rcl_publish(&publishers[id], message, nullptr);
    static_cast<void>(rc);
}

// ---- helpers for the C sequences and strings ----

template <typename Sequence, typename T, std::size_t N>
void attach(Sequence &sequence, T (&array)[N])
{
    sequence.data = array;
    sequence.size = N;
    sequence.capacity = N;
}

template <std::size_t N>
void set_string(rosidl_runtime_c__String &string, char (&buffer)[N], const char *text)
{
    snprintf(buffer, N, "%s", text);
    string.data = buffer;
    string.size = strlen(buffer);
    string.capacity = N;
}

// ROS 1 headers carried an empty frame_id; keep it a valid empty string instead of a null pointer.
char empty_text[1]{};

void set_empty_string(rosidl_runtime_c__String &string)
{
    string.data = empty_text;
    string.size = 0;
    string.capacity = sizeof empty_text;
}

// ---- actuator ----

std_msgs__msg__Int32MultiArray msg_encoder;
std_msgs__msg__Float32MultiArray msg_connection, msg_current;
int32_t msg_encoder_data[3];
float msg_connection_data[1], msg_current_data[3];

void init_actuator()
{
    attach(msg_encoder.data, msg_encoder_data);
    attach(msg_connection.data, msg_connection_data);
    attach(msg_current.data, msg_current_data);
}

void poll_actuator()
{
    actuator_controller::msg message;
    while (k_msgq_get(&actuator_controller::msgq, &message, K_NO_WAIT) == 0) {
        // ROS:[center,left,right], ROBOT:[left,center,right]
        msg_encoder.data.data[0] = message.encoder_count[1];
        msg_encoder.data.data[1] = message.encoder_count[0];
        msg_encoder.data.data[2] = message.encoder_count[2];
        publish(id_encoder, &msg_encoder);
        msg_connection.data.data[0] = message.connect * 1e-3f;
        publish(id_connection, &msg_connection);
        msg_current.data.data[0] = message.current[1] * 1e-3f;
        msg_current.data.data[1] = message.current[0] * 1e-3f;
        msg_current.data.data[2] = message.current[2] * 1e-3f;
        publish(id_current, &msg_current);
    }
}

// ---- bmu ----

lexxauto_msgs__msg__Battery msg_bmu;
sensor_msgs__msg__Temperature bmu_temps[3];
float bmu_cell_voltage[2];
char bmu_location[2];
char bmu_serial[8];

void init_bmu()
{
    attach(msg_bmu.temps, bmu_temps);
    for (auto &temp : bmu_temps)
        set_empty_string(temp.header.frame_id);
    set_empty_string(msg_bmu.state.header.frame_id);
    attach(msg_bmu.state.cell_voltage, bmu_cell_voltage);
    set_string(msg_bmu.state.location, bmu_location, "0");
    set_string(msg_bmu.state.serial_number, bmu_serial, "");
}

void poll_bmu()
{
    can_controller::msg_bmu message;
    while (k_msgq_get(&can_controller::msgq_bmu, &message, K_NO_WAIT) == 0) {
        bmu_cell_voltage[0] = message.max_cell_voltage.value;
        bmu_cell_voltage[1] = message.min_cell_voltage.value;
        snprintf(bmu_serial, sizeof bmu_serial, "%04x", message.serial);
        msg_bmu.state.serial_number.size = strlen(bmu_serial);
        if (message.mod_status1 & 0b01000000)
            msg_bmu.state.power_supply_status = sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_FULL;
        else if (message.charging_current > 0)
            msg_bmu.state.power_supply_status = sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_CHARGING;
        else
            msg_bmu.state.power_supply_status = sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_DISCHARGING;
        if (message.mod_status1 & 0b00100000 ||
            message.bmu_status == 0x07 ||
            message.bmu_status == 0x09 ||
            message.bmu_alarm1 == 0b10000010)
            msg_bmu.state.power_supply_health = sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_OVERHEAT;
        else if (message.mod_status1 & 0b00011000)
            msg_bmu.state.power_supply_health = sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_OVERVOLTAGE;
        else if (message.mod_status2 & 0b11100000)
            msg_bmu.state.power_supply_health = sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_DEAD;
        else if (message.mod_status1 & 0b10000111 ||
                 message.mod_status2 & 0b00000001 ||
                 message.bmu_status == 0xf0 ||
                 message.bmu_status == 0xf1 ||
                 message.bmu_alarm1 == 0b01111101 ||
                 message.bmu_alarm2 == 0b00000001)
            msg_bmu.state.power_supply_health = sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
        else
            msg_bmu.state.power_supply_health = sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_GOOD;
        msg_bmu.state.voltage = message.pack_voltage * 1e-3f;
        msg_bmu.state.current = message.pack_current * 1e-2f;
        msg_bmu.state.charge = message.remain_capacity * 1e-2f;
        msg_bmu.state.capacity = message.full_charge_capacity * 1e-2f;
        msg_bmu.state.design_capacity = message.design_capacity * 1e-2f;
        msg_bmu.state.percentage = message.rsoc * 1e-2f;
        msg_bmu.state.power_supply_technology = sensor_msgs__msg__BatteryState__POWER_SUPPLY_TECHNOLOGY_LION;
        msg_bmu.state.present = true;
        msg_bmu.temps.data[0].temperature = message.min_temp.value * 1e-1f;
        msg_bmu.temps.data[1].temperature = message.max_temp.value * 1e-1f;
        msg_bmu.temps.data[2].temperature = message.fet_temp * 1e-1f;
        msg_bmu.state_of_health = message.soh;
        publish(id_battery, &msg_bmu);
    }
}

// ---- board ----

std_msgs__msg__UInt8MultiArray msg_fan;
std_msgs__msg__ByteMultiArray msg_bumper;
std_msgs__msg__Bool msg_emergency;
std_msgs__msg__Byte msg_charge, msg_power;
lexxauto_msgs__msg__BoardTemperatures msg_temperature;
std_msgs__msg__UInt8 msg_charge_delay;
std_msgs__msg__Float32 msg_charge_voltage;
diagnostic_msgs__msg__DiagnosticArray msg_diagnostics;
diagnostic_msgs__msg__DiagnosticStatus diagnostics_stat;
diagnostic_msgs__msg__KeyValue diagnostics_kv[3];
uint8_t msg_fan_data[1];
uint8_t msg_bumper_data[2];

constexpr const char *corrupted_can_msg_msg = "corrupted can message received";
constexpr const char *corrupted_can_msg_error_code = "110";

char diagnostics_name[26];
char diagnostics_message[31];
char diagnostics_hardware_id[10];
char diagnostics_key0[11];
char diagnostics_key1[7];
char diagnostics_key2[4];
char diagnostics_value0[4];
char cob_id_buf[6];
char dlc_buf[2];

void init_board()
{
    attach(msg_fan.data, msg_fan_data);
    attach(msg_bumper.data, msg_bumper_data);

    set_empty_string(msg_temperature.main.header.frame_id);
    set_empty_string(msg_temperature.power.header.frame_id);
    set_empty_string(msg_temperature.linear_actuator_center.header.frame_id);
    set_empty_string(msg_temperature.linear_actuator_left.header.frame_id);
    set_empty_string(msg_temperature.linear_actuator_right.header.frame_id);
    set_empty_string(msg_temperature.charge_plus.header.frame_id);
    set_empty_string(msg_temperature.charge_minus.header.frame_id);

    // diagnostics msg keys
    set_string(diagnostics_kv[0].key, diagnostics_key0, "error code");
    set_string(diagnostics_kv[1].key, diagnostics_key1, "cob-id");
    set_string(diagnostics_kv[2].key, diagnostics_key2, "dlc");
    set_string(diagnostics_kv[0].value, diagnostics_value0, corrupted_can_msg_error_code);
    set_string(diagnostics_kv[1].value, cob_id_buf, "");
    set_string(diagnostics_kv[2].value, dlc_buf, "");

    set_empty_string(msg_diagnostics.header.frame_id);
    attach(diagnostics_stat.values, diagnostics_kv);
    set_string(diagnostics_stat.hardware_id, diagnostics_hardware_id, "mainboard");
    set_string(diagnostics_stat.name, diagnostics_name, "mainboard: can_controller");
    set_string(diagnostics_stat.message, diagnostics_message, corrupted_can_msg_msg);
    diagnostics_stat.level = diagnostic_msgs__msg__DiagnosticStatus__WARN;
    msg_diagnostics.status.data = &diagnostics_stat;
    msg_diagnostics.status.size = 1;
    msg_diagnostics.status.capacity = 1;
}

void publish_fan(const can_controller::msg_board &message)
{
    msg_fan.data.data[0] = message.fan_duty;
    publish(id_fan, &msg_fan);
}

void publish_bumper(const can_controller::msg_board &message)
{
    msg_bumper.data.data[0] = message.bumper_switch[0];
    msg_bumper.data.data[1] = message.bumper_switch[1];
    publish(id_bumper, &msg_bumper);
}

void publish_emergency(const can_controller::msg_board &message)
{
    msg_emergency.data = message.emergency_switch[0] || message.emergency_switch[1];
    publish(id_emergency_switch, &msg_emergency);
}

void publish_charge(const can_controller::msg_board &message)
{
    static constexpr uint8_t MANUAL_CHARGE_STATE{6}, AUTO_CHARGE_STATE{5};
    if (message.state == MANUAL_CHARGE_STATE)
        msg_charge.data = 2;
    else if (message.state == AUTO_CHARGE_STATE)
        msg_charge.data = 1;
    else
        msg_charge.data = 0;
    publish(id_charge_status, &msg_charge);
}

void publish_temperature(const can_controller::msg_board &message)
{
    msg_temperature.main.temperature = message.main_board_temp;
    msg_temperature.power.temperature = message.power_board_temp;
    // ROS:[center,left,right], ROBOT:[left,center,right]
    msg_temperature.linear_actuator_center.temperature = message.actuator_board_temp[1];
    msg_temperature.linear_actuator_left.temperature = message.actuator_board_temp[0];
    msg_temperature.linear_actuator_right.temperature = message.actuator_board_temp[2];
    msg_temperature.charge_plus.temperature = message.charge_connector_temp[0];
    msg_temperature.charge_minus.temperature = message.charge_connector_temp[1];
    publish(id_temperature, &msg_temperature);
}

void publish_power(const can_controller::msg_board &message)
{
    msg_power.data = message.wait_shutdown ? message.shutdown_reason : 0;
    publish(id_power_state, &msg_power);
}

void publish_charge_delay(const can_controller::msg_board &message)
{
    msg_charge_delay.data = message.charge_heartbeat_delay;
    publish(id_charge_delay, &msg_charge_delay);
}

void publish_charge_voltage(const can_controller::msg_board &message)
{
    msg_charge_voltage.data = message.charge_connector_voltage;
    publish(id_charge_voltage, &msg_charge_voltage);
}

// Host-synchronised time (the old code used nh.now()); the session is synchronised once after the node is
// created. The divisors are compile-time constants.
void fill_stamp(builtin_interfaces__msg__Time &stamp)
{
    const int64_t nanos = rmw_uros_epoch_nanos();
    constexpr int64_t nanos_per_sec{1000000000};
    stamp.sec = static_cast<int32_t>(nanos / nanos_per_sec);
    stamp.nanosec = static_cast<uint32_t>(nanos % nanos_per_sec);
}

void publish_diagnostics(const can_controller::msg_diagnostics &message)
{
    diagnostics_stat.level = diagnostic_msgs__msg__DiagnosticStatus__WARN;

    snprintf(cob_id_buf, sizeof cob_id_buf, "%d", message.cob_id);
    diagnostics_kv[1].value.size = strlen(cob_id_buf);
    snprintf(dlc_buf, sizeof dlc_buf, "%d", message.dlc);
    diagnostics_kv[2].value.size = strlen(dlc_buf);

    fill_stamp(msg_diagnostics.header.stamp);
    publish(id_diagnostics, &msg_diagnostics);
}

void poll_board()
{
    can_controller::msg_board message;
    while (k_msgq_get(&can_controller::msgq_board, &message, K_NO_WAIT) == 0) {
        publish_fan(message);
        publish_bumper(message);
        publish_emergency(message);
        publish_charge(message);
        publish_temperature(message);
        publish_power(message);
        publish_charge_delay(message);
        publish_charge_voltage(message);
    }
}

void poll_diagnostics()
{
    can_controller::msg_diagnostics message;
    while (k_msgq_get(&can_controller::msgq_diagnostics, &message, K_NO_WAIT) == 0) {
        publish_diagnostics(message);
    }
}

// ---- dfu ----

std_msgs__msg__UInt16MultiArray msg_dfu_response;
uint16_t msg_dfu_response_data[2];

void init_dfu()
{
    attach(msg_dfu_response.data, msg_dfu_response_data);
}

void poll_dfu()
{
    if (k_msgq_get(&firmware_updater::msgq_response, msg_dfu_response.data.data, K_NO_WAIT) == 0)
        publish(id_dfu_response, &msg_dfu_response);
}

// ---- imu ----

lexxauto_msgs__msg__Imu msg_imu;

void poll_imu()
{
    imu_controller::msg message;
    while (k_msgq_get(&imu_controller::msgq, &message, K_NO_WAIT) == 0) {
        msg_imu.gyro.x = message.gyro[0];
        msg_imu.gyro.y = message.gyro[1];
        msg_imu.gyro.z = message.gyro[2];
        msg_imu.accel.x = message.accel[0];
        msg_imu.accel.y = message.accel[1];
        msg_imu.accel.z = message.accel[2];
        msg_imu.ang.x = message.delta_ang[0];
        msg_imu.ang.y = message.delta_ang[1];
        msg_imu.ang.z = message.delta_ang[2];
        msg_imu.vel.x = message.delta_vel[0];
        msg_imu.vel.y = message.delta_vel[1];
        msg_imu.vel.z = message.delta_vel[2];
        publish(id_imu, &msg_imu);
    }
}

// ---- interlock ----

std_msgs__msg__Bool msg_emergency_stop_at_connected_robot;

void init_interlock()
{
    msg_emergency_stop_at_connected_robot.data = false;
}

void poll_interlock()
{
    interlock_controller::msg_connected_robot_status message;
    while (k_msgq_get(&interlock_controller::msgq_connected_robot_status, &message, K_NO_WAIT) == 0) {
        msg_emergency_stop_at_connected_robot.data = message.is_emergency_stop;
        publish(id_emergency_connected_robot, &msg_emergency_stop_at_connected_robot);
    }
}

// ---- pgv ----

lexxauto_msgs__msg__PositionGuideVision msg_pgv;
char pgv_direction[64];

void init_pgv()
{
    set_empty_string(msg_pgv.header.frame_id);
    set_string(msg_pgv.direction, pgv_direction, "Straight Ahead");
}

void publish_pgv(const pgv_controller::msg &message)
{
    float ang{static_cast<float>(message.ang) * 0.1f};
    if (ang < 180.0f)
        ang *= -1.0f;
    else
        ang = 360.0f - ang;
    float xpos{static_cast<float>(message.xps)};
    if (!message.f.tag)
        xpos = message.xp;
    float ypos{static_cast<float>(message.yps)};
    msg_pgv.angle = ang * M_PI / 180.0f;
    msg_pgv.x_pos = xpos * 1e-4f;
    msg_pgv.y_pos = ypos * 1e-4f;
    msg_pgv.direction.size = strlen(pgv_direction);
    msg_pgv.color_lane_count = message.lane;
    msg_pgv.no_color_lane = message.f.nl;
    msg_pgv.no_pos = message.f.np;
    msg_pgv.tag_detected = message.f.tag;
    msg_pgv.control_code1_detected = message.f.cc1;
    msg_pgv.control_code2_detected = message.f.cc2;
    publish(id_pgv, &msg_pgv);
}

void poll_pgv()
{
    pgv_controller::msg message;
    while (k_msgq_get(&pgv_controller::msgq, &message, K_NO_WAIT) == 0)
        publish_pgv(message);
}

// ---- tof ----

std_msgs__msg__Float64MultiArray msg_tof;
double msg_tof_data[2];

void init_tof()
{
    attach(msg_tof.data, msg_tof_data);
}

void poll_tof()
{
    tof_controller::msg message;
    while (k_msgq_get(&tof_controller::msgq, &message, K_NO_WAIT) == 0) {
        static constexpr float meter_per_volt{0.7575f};
        msg_tof.data.data[0] = message.left * 1e-3f * meter_per_volt;
        msg_tof.data.data[1] = message.right * 1e-3f * meter_per_volt;
        publish(id_downward, &msg_tof);
    }
}

// ---- uss ----

std_msgs__msg__Float64MultiArray msg_uss;
double msg_uss_data[5];

void init_uss()
{
    attach(msg_uss.data, msg_uss_data);
}

void poll_uss()
{
    uss_controller::msg message;
    while (k_msgq_get(&uss_controller::msgq, &message, K_NO_WAIT) == 0) {
        msg_uss.data.data[0] = message.front_left * 1e-3f;
        msg_uss.data.data[1] = message.front_right * 1e-3f;
        msg_uss.data.data[2] = message.left * 1e-3f;
        msg_uss.data.data[3] = message.right * 1e-3f;
        msg_uss.data.data[4] = message.back * 1e-3f;
        publish(id_ultrasonic, &msg_uss);
    }
}

// ---- towing unit ----

std_msgs__msg__UInt8MultiArray msg_towing_unit;
uint8_t msg_towing_unit_data[4];

void init_towing_unit()
{
    attach(msg_towing_unit.data, msg_towing_unit_data);
    msg_towing_unit.data.data[0] = UNLOADED;
    msg_towing_unit.data.data[1] = UNLOADED;
    msg_towing_unit.data.data[2] = V12_NG;
    msg_towing_unit.data.data[3] = V12_ON;
}

void poll_towing_unit()
{
    towing_unit_controller::msg_towing_unit_status message_pub;
    while (k_msgq_get(&towing_unit_controller::msgq_towing_unit_status, &message_pub, K_NO_WAIT) == 0) {
        msg_towing_unit.data.data[0] = message_pub.left_sw;
        msg_towing_unit.data.data[1] = message_pub.right_sw;
        msg_towing_unit.data.data[2] = message_pub.power_good;
        msg_towing_unit.data.data[3] = message_pub.power_on;
        publish(id_towing_unit, &msg_towing_unit);
    }
}

}  // namespace

}  // namespace lexxhard::microros_publishers

extern "C" int lexxhard_microros_publishers_init(rcl_node_t *node)
{
    using namespace lexxhard::microros_publishers;

    init_actuator();
    init_bmu();
    init_board();
    init_dfu();
    init_pgv();
    init_tof();
    init_uss();
    init_towing_unit();
    init_interlock();

    rcl_ret_t rc{RCL_RET_OK};
    std::size_t next{0};
#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) MICROROS_INIT_##kind(name, pkg, type)
#define MICROROS_INIT_SUB(name, pkg, type)
#define MICROROS_INIT_SRV(name, pkg, type)
#define MICROROS_INIT_PUB(name, pkg, type) \
    publishers[next] = rcl_get_zero_initialized_publisher(); \
    if (rc == RCL_RET_OK) \
        rc = rclc_publisher_init_default(&publishers[next], node, \
            ROSIDL_GET_MSG_TYPE_SUPPORT(pkg, msg, type), name); \
    ++next;
#include "microros_entities.h"
#undef MICROROS_ENTITY
    return static_cast<int>(rc);
}

extern "C" void lexxhard_microros_publishers_poll(void)
{
    using namespace lexxhard::microros_publishers;

    poll_actuator();
    poll_bmu();
    poll_board();
    poll_diagnostics();
    poll_dfu();
    poll_imu();
    poll_interlock();
    // led has no publisher
    poll_pgv();
    poll_tof();
    poll_uss();
    poll_towing_unit();
}
