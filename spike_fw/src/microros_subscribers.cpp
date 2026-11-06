/*
 * micro-ROS subscribers of the UART6 session (unit h). One to one port of the subscriber callbacks of
 * rosserial_actuator, _interlock, _pgv, _dfu, _led, _board and _towing_unit.
 *
 * The subscriptions are created from the entity table microros_entities.h (kind SUB). There is no rclc
 * executor (CONFIG_LEXXHARD_MICROROS_NO_EXECUTOR): one rcl wait set holds all subscriptions and
 * lexxhard_microros_subscribers_spin() does one non-blocking pass. No heap allocation: every sequence and
 * string of the received messages is backed by a static array.
 */
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <zephyr/kernel.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>

#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/string.h>
#include <std_msgs/msg/u_int8.h>
#include <std_msgs/msg/u_int8_multi_array.h>
#include <lexxauto_msgs/msg/led.h>
#include <lexxauto_msgs/msg/linear_actuator_control_array.h>
#include "microros_typesupport.h"

#include "microros_subscribers.h"
#include "microros_publishers.h"
#include "microros_board_store.hpp"

#include "actuator_controller.hpp"
#include "can_controller.hpp"
#include "firmware_updater.hpp"
#include "interlock_controller.hpp"
#include "led_controller.hpp"
#include "pgv_controller.hpp"
#include "towing_unit_controller.hpp"

namespace lexxhard::microros_subscribers {

namespace {

// ---- subscription table (generated from microros_entities.h) ----

#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) MICROROS_TOPIC_##kind(name)
#define MICROROS_TOPIC_SUB(name) name,
#define MICROROS_TOPIC_PUB(name)
#define MICROROS_TOPIC_SRV(name)
constexpr const char *const sub_topics[] = {
#include "microros_entities.h"
};
#undef MICROROS_ENTITY

constexpr std::size_t num_sub = sizeof sub_topics / sizeof sub_topics[0];

constexpr bool same_string(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

constexpr std::size_t sub_index(const char *topic)
{
    for (std::size_t i{0}; i < num_sub; ++i) {
        if (same_string(sub_topics[i], topic))
            return i;
    }
    return num_sub;
}

#define SUB_ID(ident, topic) \
    constexpr std::size_t ident = sub_index(topic); \
    static_assert(ident < num_sub, "topic is not a subscriber in microros_entities.h: " topic)

SUB_ID(id_actuator, "/body_control/linear_actuator");
SUB_ID(id_emergency_at_amr, "/control/emergency_stop_at_amr");
SUB_ID(id_pgv_dir, "/sensor_set/pgv_dir");
SUB_ID(id_dfu_data, "/lexxhard/dfu_data");
SUB_ID(id_led, "/body_control/led");
SUB_ID(id_led_direct, "/body_control/led_direct");
SUB_ID(id_request_emergency, "/control/request_emergency_stop");
SUB_ID(id_request_poweroff, "/control/request_power_off");
SUB_ID(id_request_lockdown, "/control/request_lockdown");
SUB_ID(id_setup, "/lexxhard/setup");
SUB_ID(id_heartbeat, "/lexxhard/mainboard_messenger_heartbeat");
SUB_ID(id_towing_power_on, "/control/towing_unit_power_on");

static_assert(num_sub == 12, "unexpected number of subscribers");

// A subscription, its static message and the callback that is called with the received message.
struct slot {
    void *message;
    void (*prepare)();
    void (*callback)(const void *);
};

rcl_subscription_t subscriptions[num_sub];
slot slots[num_sub];
rcl_wait_set_t wait_set;

template <typename Message, void (*Callback)(const Message &)>
void thunk(const void *message)
{
    Callback(*static_cast<const Message *>(message));
}

// ---- message buffers (static) ----

constexpr std::size_t actuator_capacity{16};
constexpr std::size_t dfu_capacity{sizeof(firmware_updater::packet_array)};
constexpr std::size_t string_capacity{512};
constexpr std::size_t layout_dims{4};
constexpr std::size_t layout_label_capacity{16};

lexxauto_msgs__msg__LinearActuatorControlArray msg_actuator;
lexxauto_msgs__msg__LinearActuatorControl msg_actuator_data[actuator_capacity];

std_msgs__msg__Bool msg_emergency_at_amr;
std_msgs__msg__UInt8 msg_pgv_dir;

std_msgs__msg__UInt8MultiArray msg_dfu;
uint8_t msg_dfu_data[dfu_capacity];
std_msgs__msg__MultiArrayDimension msg_dfu_dims[layout_dims];
char msg_dfu_labels[layout_dims][layout_label_capacity];

std_msgs__msg__String msg_led;
char msg_led_data[string_capacity];
lexxauto_msgs__msg__Led msg_led_direct;

std_msgs__msg__Bool msg_request_emergency, msg_request_poweroff, msg_request_lockdown;
std_msgs__msg__String msg_setup;
char msg_setup_data[string_capacity];
std_msgs__msg__Bool msg_heartbeat;
std_msgs__msg__UInt8 msg_towing_power_on;

void reset_string(rosidl_runtime_c__String &string, char *buffer, std::size_t capacity)
{
    buffer[0] = '\0';
    string.data = buffer;
    string.size = 0;
    string.capacity = capacity;
}

// The sizes are rewritten by the deserializer; restore them before every take.
void prepare_actuator()
{
    std::memset(msg_actuator_data, 0, sizeof msg_actuator_data);
    msg_actuator.actuators.data = msg_actuator_data;
    msg_actuator.actuators.size = 0;
    msg_actuator.actuators.capacity = actuator_capacity;
}

void prepare_dfu()
{
    msg_dfu.data.data = msg_dfu_data;
    msg_dfu.data.size = 0;
    msg_dfu.data.capacity = dfu_capacity;
    msg_dfu.layout.dim.data = msg_dfu_dims;
    msg_dfu.layout.dim.size = 0;
    msg_dfu.layout.dim.capacity = layout_dims;
    for (std::size_t i{0}; i < layout_dims; ++i)
        reset_string(msg_dfu_dims[i].label, msg_dfu_labels[i], layout_label_capacity);
}

void prepare_led()
{
    reset_string(msg_led.data, msg_led_data, string_capacity);
}

void prepare_setup()
{
    reset_string(msg_setup.data, msg_setup_data, string_capacity);
}

// ---- callbacks (ports of the rosserial callbacks) ----

void callback_actuator(const lexxauto_msgs__msg__LinearActuatorControlArray &req)
{
    actuator_controller::msg_control message;
    // ROS:[center,left,right], ROBOT:[left,center,right]
    // A sequence smaller than 3 leaves the missing entries at zero (direction STOP, power 0).
    message.actuators[0].direction = req.actuators.data[1].direction;
    message.actuators[1].direction = req.actuators.data[0].direction;
    message.actuators[2].direction = req.actuators.data[2].direction;
    message.actuators[0].power = req.actuators.data[1].power;
    message.actuators[1].power = req.actuators.data[0].power;
    message.actuators[2].power = req.actuators.data[2].power;
    while (k_msgq_put(&actuator_controller::msgq_control, &message, K_NO_WAIT) != 0)
        k_msgq_purge(&actuator_controller::msgq_control);
}

void callback_emergency_stop_at_amr(const std_msgs__msg__Bool &msg)
{
    interlock_controller::msg_amr_status message{msg.data};
    while (k_msgq_put(&interlock_controller::msgq_amr_status, &message, K_NO_WAIT) != 0)
        k_msgq_purge(&interlock_controller::msgq_amr_status);
}

void callback_pgv(const std_msgs__msg__UInt8 &req)
{
    switch (req.data) {
    case 0:
        lexxhard_microros_pgv_set_direction("No lane is selected");
        break;
    case 1:
        lexxhard_microros_pgv_set_direction("Right lane is selected");
        break;
    case 2:
        lexxhard_microros_pgv_set_direction("Left lane is selected");
        break;
    case 3:
        lexxhard_microros_pgv_set_direction("Straight Ahead");
        break;
    }
    pgv_controller::msg_control ros2pgv;
    ros2pgv.dir_command = req.data;
    while (k_msgq_put(&pgv_controller::msgq_control, &ros2pgv, K_NO_WAIT) != 0)
        k_msgq_purge(&pgv_controller::msgq_control);
}

void callback_dfu(const std_msgs__msg__UInt8MultiArray &packet)
{
    // Fixed size copy, data_length is never read (as the rosserial callback).
    while (k_msgq_put(&firmware_updater::msgq_data, packet.data.data, K_NO_WAIT) != 0)
        k_msgq_purge(&firmware_updater::msgq_data);
}

void callback_led_string(const std_msgs__msg__String &req)
{
    led_controller::msg message{req.data.data};
    while (k_msgq_put(&led_controller::msgq, &message, K_NO_WAIT) != 0)
        k_msgq_purge(&led_controller::msgq);
}

void callback_led_direct(const lexxauto_msgs__msg__Led &req)
{
    led_controller::msg message;
    message.pattern = led_controller::msg::RGB;
    message.interrupt_ms = 0;
    message.rgb[0] = req.r;
    message.rgb[1] = req.g;
    message.rgb[2] = req.b;
    while (k_msgq_put(&led_controller::msgq, &message, K_NO_WAIT) != 0)
        k_msgq_purge(&led_controller::msgq);
}

bool prev_emergency_request{false};

void put_ros2board()
{
    auto ros2board{ros_board_store::get_ros2board()};
    while (k_msgq_put(&can_controller::msgq_control, &ros2board, K_NO_WAIT) != 0)
        k_msgq_purge(&can_controller::msgq_control);
}

void callback_emergency(const std_msgs__msg__Bool &req)
{
    ros_board_store::set_emergency_stop(req.data);

    if (prev_emergency_request && !req.data) {
        ros_board_store::set_auto_charge_request_enable(false);
    }
    prev_emergency_request = req.data;

    put_ros2board();
}

void callback_poweroff(const std_msgs__msg__Bool &req)
{
    ros_board_store::set_power_off(req.data);
    put_ros2board();
}

void callback_lockdown(const std_msgs__msg__Bool &req)
{
    ros_board_store::set_lockdown(req.data);
    put_ros2board();
}

void callback_lexxhard(const std_msgs__msg__String &req)
{
    if (strncmp(req.data.data, "wheel_", 6) == 0) {
        ros_board_store::set_wheel_power_off(strcmp(req.data.data, "wheel_poweroff") == 0);
    }
    put_ros2board();
}

void callback_messenger(const std_msgs__msg__Bool &)
{
    put_ros2board();
}

void callback_towing_unit_power_on(const std_msgs__msg__UInt8 &msg)
{
    towing_unit_controller::msg_towing_unit_status message_sub{};
    message_sub.power_on = msg.data;

    while (k_msgq_put(&towing_unit_controller::msgq_towing_unit_power_on, &message_sub, K_NO_WAIT) != 0) {
        k_msgq_purge(&towing_unit_controller::msgq_towing_unit_power_on);
    }
}

void prepare_none()
{
}

void init_slots()
{
    prepare_actuator();
    prepare_dfu();
    prepare_led();
    prepare_setup();

    slots[id_actuator] = {&msg_actuator, prepare_actuator,
        thunk<lexxauto_msgs__msg__LinearActuatorControlArray, callback_actuator>};
    slots[id_emergency_at_amr] = {&msg_emergency_at_amr, prepare_none,
        thunk<std_msgs__msg__Bool, callback_emergency_stop_at_amr>};
    slots[id_pgv_dir] = {&msg_pgv_dir, prepare_none, thunk<std_msgs__msg__UInt8, callback_pgv>};
    slots[id_dfu_data] = {&msg_dfu, prepare_dfu, thunk<std_msgs__msg__UInt8MultiArray, callback_dfu>};
    slots[id_led] = {&msg_led, prepare_led, thunk<std_msgs__msg__String, callback_led_string>};
    slots[id_led_direct] = {&msg_led_direct, prepare_none, thunk<lexxauto_msgs__msg__Led, callback_led_direct>};
    slots[id_request_emergency] = {&msg_request_emergency, prepare_none,
        thunk<std_msgs__msg__Bool, callback_emergency>};
    slots[id_request_poweroff] = {&msg_request_poweroff, prepare_none,
        thunk<std_msgs__msg__Bool, callback_poweroff>};
    slots[id_request_lockdown] = {&msg_request_lockdown, prepare_none,
        thunk<std_msgs__msg__Bool, callback_lockdown>};
    slots[id_setup] = {&msg_setup, prepare_setup, thunk<std_msgs__msg__String, callback_lexxhard>};
    slots[id_heartbeat] = {&msg_heartbeat, prepare_none, thunk<std_msgs__msg__Bool, callback_messenger>};
    slots[id_towing_power_on] = {&msg_towing_power_on, prepare_none,
        thunk<std_msgs__msg__UInt8, callback_towing_unit_power_on>};
}

}  // namespace

}  // namespace lexxhard::microros_subscribers

extern "C" int lexxhard_microros_subscribers_init(rcl_node_t *node, rcl_context_t *context)
{
    using namespace lexxhard::microros_subscribers;

    init_slots();

    rcl_ret_t rc{RCL_RET_OK};
    std::size_t next{0};
#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) MICROROS_INIT_##kind(name, pkg, type)
#define MICROROS_INIT_PUB(name, pkg, type)
#define MICROROS_INIT_SRV(name, pkg, type)
#define MICROROS_INIT_SUB(name, pkg, type) \
    subscriptions[next] = rcl_get_zero_initialized_subscription(); \
    if (rc == RCL_RET_OK) \
        rc = rclc_subscription_init_default(&subscriptions[next], node, \
            ROSIDL_GET_MSG_TYPE_SUPPORT(pkg, msg, type), name); \
    ++next;
#include "microros_entities.h"
#undef MICROROS_ENTITY

    if (rc != RCL_RET_OK)
        return static_cast<int>(rc);

    wait_set = rcl_get_zero_initialized_wait_set();
    rc = rcl_wait_set_init(&wait_set, num_sub, 0, 0, 0, 0, 0, context, rcl_get_default_allocator());
    return static_cast<int>(rc);
}

extern "C" void lexxhard_microros_subscribers_spin(void)
{
    using namespace lexxhard::microros_subscribers;

    // Failures of wait and take are stored and ignored; the loop is never aborted.
    rcl_ret_t rc{rcl_wait_set_clear(&wait_set)};
    for (std::size_t i{0}; i < num_sub; ++i) {
        rc = rcl_wait_set_add_subscription(&wait_set, &subscriptions[i], nullptr);
    }
    rc = rcl_wait(&wait_set, 0);

    for (std::size_t i{0}; i < num_sub; ++i) {
        if (wait_set.subscriptions[i] == nullptr)
            continue;
        slots[i].prepare();
        rc = rcl_take(&subscriptions[i], slots[i].message, nullptr, nullptr);
        if (rc == RCL_RET_OK) {
            // The deserializer NUL-terminates when the capacity allows; the callbacks use C strings.
            msg_led_data[string_capacity - 1] = '\0';
            msg_setup_data[string_capacity - 1] = '\0';
            slots[i].callback(slots[i].message);
        }
    }
    static_cast<void>(rc);
}
