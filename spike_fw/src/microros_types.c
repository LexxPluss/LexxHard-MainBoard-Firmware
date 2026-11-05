/*
 * Size spike: one micro-ROS entity per message and service type that the MainBoard firmware uses, so that the
 * linker keeps the type support code of every type. Based on the sample app of micro-ROS/micro_ros_zephyr_module
 * (Apache-2.0). Build check and size measurement only; it is not meant to run.
 */
#include <stdio.h>

#include <zephyr/kernel.h>

#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include "microros_transports.h"
#include "microros_types.h"

#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/byte.h>
#include <std_msgs/msg/byte_multi_array.h>
#include <std_msgs/msg/float32.h>
#include <std_msgs/msg/float32_multi_array.h>
#include <std_msgs/msg/float64_multi_array.h>
#include <std_msgs/msg/int32_multi_array.h>
#include <std_msgs/msg/string.h>
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
#include <lexxauto_msgs/msg/led.h>
#include <lexxauto_msgs/msg/linear_actuator_control_array.h>
#include <lexxauto_msgs/msg/position_guide_vision.h>
#include <std_srvs/srv/set_bool.h>
#include <lexxauto_msgs/srv/init_linear_actuator.h>
#include <lexxauto_msgs/srv/linear_actuator_location.h>

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if (temp_rc != RCL_RET_OK) { printf("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); for (;;) {}; } }
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if (temp_rc != RCL_RET_OK) { printf("Failed status on line %d: %d. Continuing.\n", __LINE__, (int)temp_rc); } }

/* the 22 message types the firmware publishes or subscribes to (X(package, type)) */
#define MSG_LIST(X) \
	X(std_msgs, Bool) \
	X(std_msgs, Byte) \
	X(std_msgs, ByteMultiArray) \
	X(std_msgs, Float32) \
	X(std_msgs, Float32MultiArray) \
	X(std_msgs, Float64MultiArray) \
	X(std_msgs, Int32MultiArray) \
	X(std_msgs, String) \
	X(std_msgs, UInt16MultiArray) \
	X(std_msgs, UInt8) \
	X(std_msgs, UInt8MultiArray) \
	X(sensor_msgs, BatteryState) \
	X(sensor_msgs, Temperature) \
	X(diagnostic_msgs, DiagnosticArray) \
	X(diagnostic_msgs, DiagnosticStatus) \
	X(diagnostic_msgs, KeyValue) \
	X(lexxauto_msgs, Battery) \
	X(lexxauto_msgs, BoardTemperatures) \
	X(lexxauto_msgs, Imu) \
	X(lexxauto_msgs, Led) \
	X(lexxauto_msgs, LinearActuatorControlArray) \
	X(lexxauto_msgs, PositionGuideVision)

#define DECLARE(pkg, T) \
	static rcl_publisher_t pub_##pkg##_##T; \
	static pkg##__msg__##T msg_##pkg##_##T;
MSG_LIST(DECLARE)

static std_msgs__msg__Bool sub_msg_bool;
static std_msgs__msg__UInt8 sub_msg_u8;
static rcl_subscription_t sub_bool, sub_u8;

static rcl_service_t srv_set_bool, srv_init_actuator, srv_actuator_location;
static std_srvs__srv__SetBool_Request req_set_bool;
static std_srvs__srv__SetBool_Response res_set_bool;
static lexxauto_msgs__srv__InitLinearActuator_Request req_init_actuator;
static lexxauto_msgs__srv__InitLinearActuator_Response res_init_actuator;
static lexxauto_msgs__srv__LinearActuatorLocation_Request req_actuator_location;
static lexxauto_msgs__srv__LinearActuatorLocation_Response res_actuator_location;

static void timer_callback(rcl_timer_t *timer, int64_t last_call_time)
{
	RCLC_UNUSED(last_call_time);
	if (timer == NULL) {
		return;
	}
#define PUBLISH(pkg, T) RCSOFTCHECK(rcl_publish(&pub_##pkg##_##T, &msg_##pkg##_##T, NULL));
	MSG_LIST(PUBLISH)
}

static void on_bool(const void *msg)
{
	const std_msgs__msg__Bool *m = msg;
	printf("bool %d\n", (int)m->data);
}

static void on_u8(const void *msg)
{
	const std_msgs__msg__UInt8 *m = msg;
	printf("u8 %u\n", (unsigned)m->data);
}

static void on_set_bool(const void *req, void *res)
{
	const std_srvs__srv__SetBool_Request *rq = req;
	std_srvs__srv__SetBool_Response *rs = res;
	rs->success = rq->data;
}

static void on_init_actuator(const void *req, void *res)
{
	const lexxauto_msgs__srv__InitLinearActuator_Request *rq = req;
	lexxauto_msgs__srv__InitLinearActuator_Response *rs = res;
	rs->success = rq->directions.data.size > 0;
}

static void on_actuator_location(const void *req, void *res)
{
	const lexxauto_msgs__srv__LinearActuatorLocation_Request *rq = req;
	lexxauto_msgs__srv__LinearActuatorLocation_Response *rs = res;
	rs->success = rq->location.data.size > 0;
}

void lexxhard_microros_run(void)
{
	rmw_uros_set_custom_transport(
		LEXXHARD_MICRO_ROS_FRAMING_REQUIRED,
		(void *) &lexxhard_default_params,
		lexxhard_transport_open,
		lexxhard_transport_close,
		lexxhard_transport_write,
		lexxhard_transport_read
	);

	rcl_allocator_t allocator = rcl_get_default_allocator();
	rclc_support_t support;
	RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

	rcl_node_t node;
	RCCHECK(rclc_node_init_default(&node, "spike_microros_types", "", &support));

#define INIT(pkg, T) \
	pkg##__msg__##T##__init(&msg_##pkg##_##T); \
	RCCHECK(rclc_publisher_init_default(&pub_##pkg##_##T, &node, \
		ROSIDL_GET_MSG_TYPE_SUPPORT(pkg, msg, T), #pkg "_" #T));
	MSG_LIST(INIT)

	std_msgs__msg__Bool__init(&sub_msg_bool);
	std_msgs__msg__UInt8__init(&sub_msg_u8);
	RCCHECK(rclc_subscription_init_default(&sub_bool, &node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool), "sub_bool"));
	RCCHECK(rclc_subscription_init_default(&sub_u8, &node,
		ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt8), "sub_u8"));

	std_srvs__srv__SetBool_Request__init(&req_set_bool);
	std_srvs__srv__SetBool_Response__init(&res_set_bool);
	lexxauto_msgs__srv__InitLinearActuator_Request__init(&req_init_actuator);
	lexxauto_msgs__srv__InitLinearActuator_Response__init(&res_init_actuator);
	lexxauto_msgs__srv__LinearActuatorLocation_Request__init(&req_actuator_location);
	lexxauto_msgs__srv__LinearActuatorLocation_Response__init(&res_actuator_location);
	RCCHECK(rclc_service_init_default(&srv_set_bool, &node,
		ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, SetBool), "set_bool"));
	RCCHECK(rclc_service_init_default(&srv_init_actuator, &node,
		ROSIDL_GET_SRV_TYPE_SUPPORT(lexxauto_msgs, srv, InitLinearActuator), "init_actuator"));
	RCCHECK(rclc_service_init_default(&srv_actuator_location, &node,
		ROSIDL_GET_SRV_TYPE_SUPPORT(lexxauto_msgs, srv, LinearActuatorLocation), "actuator_location"));

	rcl_timer_t timer;
	RCCHECK(rclc_timer_init_default(&timer, &support, RCL_MS_TO_NS(1000), timer_callback));

	rclc_executor_t executor;
	RCCHECK(rclc_executor_init(&executor, &support.context, 6, &allocator));
	RCCHECK(rclc_executor_add_timer(&executor, &timer));
	RCCHECK(rclc_executor_add_subscription(&executor, &sub_bool, &sub_msg_bool, on_bool, ON_NEW_DATA));
	RCCHECK(rclc_executor_add_subscription(&executor, &sub_u8, &sub_msg_u8, on_u8, ON_NEW_DATA));
	RCCHECK(rclc_executor_add_service(&executor, &srv_set_bool, &req_set_bool, &res_set_bool, on_set_bool));
	RCCHECK(rclc_executor_add_service(&executor, &srv_init_actuator, &req_init_actuator,
		&res_init_actuator, on_init_actuator));
	RCCHECK(rclc_executor_add_service(&executor, &srv_actuator_location, &req_actuator_location,
		&res_actuator_location, on_actuator_location));

	while (1) {
		rclc_executor_spin_some(&executor, 100);
		k_msleep(100);
	}
}
