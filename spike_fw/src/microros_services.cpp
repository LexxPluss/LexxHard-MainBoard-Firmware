/*
 * micro-ROS services of the UART2 session. One to one port of the service callbacks of
 * rosserial_actuator_service.hpp (location, init) and rosserial_board_service.hpp (auto charge request).
 *
 * The services are created from the entity table microros_entities.h (kind SRV). There is no rclc executor:
 * one rcl wait set holds all services and lexxhard_microros_services_spin() does one non-blocking pass.
 * No heap allocation: every sequence and string of the requests and responses is backed by a static array.
 */
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <zephyr/kernel.h>

#include <rcl/rcl.h>
#include <rmw/types.h>

#include <std_msgs/msg/int8_multi_array.h>
#include <std_msgs/msg/u_int8_multi_array.h>
#include <std_srvs/srv/set_bool.h>
#include <lexxauto_msgs/srv/init_linear_actuator.h>
#include <lexxauto_msgs/srv/linear_actuator_location.h>
#include "microros_typesupport.h"

#include "microros_services.h"
#include "microros_board_store.hpp"

#include "actuator_controller.hpp"

namespace lexxhard::microros_services {

namespace {

// ---- service table (generated from microros_entities.h) ----

#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) MICROROS_TOPIC_##kind(name)
#define MICROROS_TOPIC_SRV(name) name,
#define MICROROS_TOPIC_PUB(name)
#define MICROROS_TOPIC_SUB(name)
constexpr const char *const srv_names[] = {
#include "microros_entities.h"
};
#undef MICROROS_ENTITY

constexpr std::size_t num_srv = sizeof srv_names / sizeof srv_names[0];

constexpr bool same_string(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

constexpr std::size_t srv_index(const char *name)
{
    for (std::size_t i{0}; i < num_srv; ++i) {
        if (same_string(srv_names[i], name))
            return i;
    }
    return num_srv;
}

#define SRV_ID(ident, name) \
    constexpr std::size_t ident = srv_index(name); \
    static_assert(ident < num_srv, "name is not a service in microros_entities.h: " name)

SRV_ID(id_location, "/body_control/linear_actuator_location");
SRV_ID(id_init, "/body_control/init_linear_actuator");
SRV_ID(id_auto_charge, "/control/auto_charge_request/enable");

static_assert(num_srv == 3, "unexpected number of services");

// A service, its static request/response and the handler that fills the response from the request.
struct slot {
    void *request;
    void *response;
    void (*prepare)();
    void (*handler)(const void *, void *);
};

rcl_service_t services[num_srv];
slot slots[num_srv];
rcl_wait_set_t wait_set;

template <typename Request, typename Response, void (*Handler)(const Request &, Response &)>
void thunk(const void *request, void *response)
{
    Handler(*static_cast<const Request *>(request), *static_cast<Response *>(response));
}

// ---- message buffers (static) ----

constexpr std::size_t elements{3};
constexpr std::size_t layout_dims{4};
constexpr std::size_t layout_label_capacity{16};

lexxauto_msgs__srv__LinearActuatorLocation_Request req_location;
lexxauto_msgs__srv__LinearActuatorLocation_Response res_location;
uint8_t req_location_location[elements];
uint8_t req_location_power[elements];
uint8_t res_location_detail[elements];

lexxauto_msgs__srv__InitLinearActuator_Request req_init;
lexxauto_msgs__srv__InitLinearActuator_Response res_init;
int8_t req_init_directions[elements];

std_srvs__srv__SetBool_Request req_auto_charge;
std_srvs__srv__SetBool_Response res_auto_charge;
char res_auto_charge_message[1];

// Backing of the MultiArrayLayout of the request arrays (the deserializer needs room for the dimensions).
std_msgs__msg__MultiArrayDimension layout_location_dims[layout_dims];
char layout_location_labels[layout_dims][layout_label_capacity];
std_msgs__msg__MultiArrayDimension layout_power_dims[layout_dims];
char layout_power_labels[layout_dims][layout_label_capacity];
std_msgs__msg__MultiArrayDimension layout_directions_dims[layout_dims];
char layout_directions_labels[layout_dims][layout_label_capacity];

void reset_string(rosidl_runtime_c__String &string, char *buffer, std::size_t capacity)
{
    buffer[0] = '\0';
    string.data = buffer;
    string.size = 0;
    string.capacity = capacity;
}

void reset_layout(std_msgs__msg__MultiArrayLayout &layout, std_msgs__msg__MultiArrayDimension *dims,
    char (*labels)[layout_label_capacity])
{
    layout.dim.data = dims;
    layout.dim.size = 0;
    layout.dim.capacity = layout_dims;
    layout.data_offset = 0;
    for (std::size_t i{0}; i < layout_dims; ++i)
        reset_string(dims[i].label, labels[i], layout_label_capacity);
}

// The sizes are rewritten by the deserializer; restore them before every take.
void prepare_location()
{
    std::memset(req_location_location, 0, sizeof req_location_location);
    std::memset(req_location_power, 0, sizeof req_location_power);
    req_location.location.data.data = req_location_location;
    req_location.location.data.size = 0;
    req_location.location.data.capacity = elements;
    req_location.power.data.data = req_location_power;
    req_location.power.data.size = 0;
    req_location.power.data.capacity = elements;
    reset_layout(req_location.location.layout, layout_location_dims, layout_location_labels);
    reset_layout(req_location.power.layout, layout_power_dims, layout_power_labels);
    // The response is serialized only; its layout stays empty.
    // The rosserial callback wrote res.detail.data[0..2] through a null data pointer. Here the response
    // sequence is backed by a static array, so there is no null write.
    res_location.detail.data.data = res_location_detail;
    res_location.detail.data.size = 0;
    res_location.detail.data.capacity = elements;
    res_location.detail.layout.dim.data = nullptr;
    res_location.detail.layout.dim.size = 0;
    res_location.detail.layout.dim.capacity = 0;
    res_location.detail.layout.data_offset = 0;
    res_location.success = false;
}

void prepare_init()
{
    std::memset(req_init_directions, 0, sizeof req_init_directions);
    req_init.directions.data.data = req_init_directions;
    req_init.directions.data.size = 0;
    req_init.directions.data.capacity = elements;
    reset_layout(req_init.directions.layout, layout_directions_dims, layout_directions_labels);
    res_init.success = false;
}

void prepare_auto_charge()
{
    req_auto_charge.data = false;
    reset_string(res_auto_charge.message, res_auto_charge_message, sizeof res_auto_charge_message);
    res_auto_charge.success = false;
}

// ---- handlers (ports of the rosserial callbacks) ----

// The rosserial callbacks read element 0..2 unchecked. Here a sequence smaller than 3 reads the missing
// elements as 0 (the static backing array is zeroed before every take); nothing is read beyond size.
template <typename T, typename Sequence>
T element(const Sequence &sequence, std::size_t index)
{
    return index < sequence.size ? sequence.data[index] : T{0};
}

void handler_location(const lexxauto_msgs__srv__LinearActuatorLocation_Request &req,
    lexxauto_msgs__srv__LinearActuatorLocation_Response &res)
{
    // ROS:[center,left,right], ROBOT:[left,center,right]
    const uint8_t location[3]{
        element<uint8_t>(req.location.data, 1),
        element<uint8_t>(req.location.data, 0),
        element<uint8_t>(req.location.data, 2)
    };
    const uint8_t power[3]{
        element<uint8_t>(req.power.data, 1),
        element<uint8_t>(req.power.data, 0),
        element<uint8_t>(req.power.data, 2)
    };
    uint8_t detail[3]{};
    res.success = actuator_controller::to_location(location, power, detail) == 0;
    res.detail.data.data[0] = detail[1];
    res.detail.data.data[1] = detail[0];
    res.detail.data.data[2] = detail[2];
    res.detail.data.size = sizeof detail / sizeof detail[0];
}

void handler_init(const lexxauto_msgs__srv__InitLinearActuator_Request &req,
    lexxauto_msgs__srv__InitLinearActuator_Response &res)
{
    // ROS:[center,left,right], ROBOT:[left,center,right]
    const int8_t directions[3]{
        element<int8_t>(req.directions.data, 1),
        element<int8_t>(req.directions.data, 0),
        element<int8_t>(req.directions.data, 2)
    };
    res.success = actuator_controller::init_location(directions) == 0;
}

void handler_auto_charge(const std_srvs__srv__SetBool_Request &req, std_srvs__srv__SetBool_Response &res)
{
    ros_board_store::set_auto_charge_request_enable(req.data);
    // Don't put ros2board into the queue here. It is put as a part of the heartbeat.
    res.success = true;
}

void prepare_all()
{
    prepare_location();
    prepare_init();
    prepare_auto_charge();
}

void init_slots()
{
    prepare_all();

    slots[id_location] = {&req_location, &res_location, prepare_location,
        thunk<lexxauto_msgs__srv__LinearActuatorLocation_Request,
            lexxauto_msgs__srv__LinearActuatorLocation_Response, handler_location>};
    slots[id_init] = {&req_init, &res_init, prepare_init,
        thunk<lexxauto_msgs__srv__InitLinearActuator_Request,
            lexxauto_msgs__srv__InitLinearActuator_Response, handler_init>};
    slots[id_auto_charge] = {&req_auto_charge, &res_auto_charge, prepare_auto_charge,
        thunk<std_srvs__srv__SetBool_Request, std_srvs__srv__SetBool_Response, handler_auto_charge>};
}

}  // namespace

}  // namespace lexxhard::microros_services

extern "C" int lexxhard_microros_services_init(rcl_node_t *node, rcl_context_t *context)
{
    using namespace lexxhard::microros_services;

    init_slots();

    rcl_ret_t rc{RCL_RET_OK};
    std::size_t next{0};
#define MICROROS_ENTITY(kind, name, pkg, type, port, qos) MICROROS_INIT_##kind(name, pkg, type)
#define MICROROS_INIT_PUB(name, pkg, type)
#define MICROROS_INIT_SUB(name, pkg, type)
#define MICROROS_INIT_SRV(name, pkg, type) \
    services[next] = rcl_get_zero_initialized_service(); \
    if (rc == RCL_RET_OK) { \
        const rcl_service_options_t options{rcl_service_get_default_options()}; \
        rc = rcl_service_init(&services[next], node, \
            ROSIDL_GET_SRV_TYPE_SUPPORT(pkg, srv, type), name, &options); \
    } \
    ++next;
#include "microros_entities.h"
#undef MICROROS_ENTITY

    if (rc != RCL_RET_OK)
        return static_cast<int>(rc);

    wait_set = rcl_get_zero_initialized_wait_set();
    rc = rcl_wait_set_init(&wait_set, 0, 0, 0, 0, num_srv, 0, context, rcl_get_default_allocator());
    return static_cast<int>(rc);
}

extern "C" void lexxhard_microros_services_spin(void)
{
    using namespace lexxhard::microros_services;

    // Failures of wait, take and send are stored and ignored; the loop is never aborted.
    rcl_ret_t rc{rcl_wait_set_clear(&wait_set)};
    for (std::size_t i{0}; i < num_srv; ++i) {
        rc = rcl_wait_set_add_service(&wait_set, &services[i], nullptr);
    }
    rc = rcl_wait(&wait_set, 0);

    for (std::size_t i{0}; i < num_srv; ++i) {
        if (wait_set.services[i] == nullptr)
            continue;
        slots[i].prepare();
        rmw_request_id_t header;
        rc = rcl_take_request(&services[i], &header, slots[i].request);
        if (rc == RCL_RET_OK) {
            slots[i].handler(slots[i].request, slots[i].response);
            rc = rcl_send_response(&services[i], &header, slots[i].response);
        }
    }
    static_cast<void>(rc);
}
