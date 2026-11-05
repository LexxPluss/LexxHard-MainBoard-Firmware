/*
 * Size measurement only (CONFIG_LEXXHARD_MICROROS_DIRECT_TYPESUPPORT): ask the micro-XRCE-DDS type support
 * directly instead of the rosidl_typesupport_c dispatch layer. The rosidl_typesupport_c handle is only a
 * dispatch table; rmw_microxrcedds asks it for the handle whose identifier is
 * ROSIDL_TYPESUPPORT_MICROXRCEDDS_C__IDENTIFIER_VALUE, and a handle that already carries that identifier is
 * returned as is by get_message_typesupport_handle_function / get_service_typesupport_handle_function.
 */
#ifndef LEXXHARD_MICROROS_TYPESUPPORT_H
#define LEXXHARD_MICROROS_TYPESUPPORT_H

#ifdef CONFIG_LEXXHARD_MICROROS_DIRECT_TYPESUPPORT

#include <rosidl_runtime_c/message_type_support_struct.h>
#include <rosidl_runtime_c/service_type_support_struct.h>
#include <rosidl_typesupport_interface/macros.h>

#include <std_msgs/msg/detail/bool__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/byte__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/byte_multi_array__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/float32__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/float32_multi_array__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/float64_multi_array__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/int32_multi_array__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/string__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/u_int16_multi_array__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/u_int8__rosidl_typesupport_microxrcedds_c.h>
#include <std_msgs/msg/detail/u_int8_multi_array__rosidl_typesupport_microxrcedds_c.h>
#include <sensor_msgs/msg/detail/battery_state__rosidl_typesupport_microxrcedds_c.h>
#include <sensor_msgs/msg/detail/temperature__rosidl_typesupport_microxrcedds_c.h>
#include <diagnostic_msgs/msg/detail/diagnostic_array__rosidl_typesupport_microxrcedds_c.h>
#include <diagnostic_msgs/msg/detail/diagnostic_status__rosidl_typesupport_microxrcedds_c.h>
#include <diagnostic_msgs/msg/detail/key_value__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/msg/detail/battery__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/msg/detail/board_temperatures__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/msg/detail/imu__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/msg/detail/led__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/msg/detail/linear_actuator_control_array__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/msg/detail/position_guide_vision__rosidl_typesupport_microxrcedds_c.h>
#include <std_srvs/srv/detail/set_bool__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/srv/detail/init_linear_actuator__rosidl_typesupport_microxrcedds_c.h>
#include <lexxauto_msgs/srv/detail/linear_actuator_location__rosidl_typesupport_microxrcedds_c.h>

#undef ROSIDL_GET_MSG_TYPE_SUPPORT
#define ROSIDL_GET_MSG_TYPE_SUPPORT(PkgName, MsgSubfolder, MsgName) \
	ROSIDL_TYPESUPPORT_INTERFACE__MESSAGE_SYMBOL_NAME( \
		rosidl_typesupport_microxrcedds_c, PkgName, MsgSubfolder, MsgName)()

#undef ROSIDL_GET_SRV_TYPE_SUPPORT
#define ROSIDL_GET_SRV_TYPE_SUPPORT(PkgName, SrvSubfolder, SrvName) \
	ROSIDL_TYPESUPPORT_INTERFACE__SERVICE_SYMBOL_NAME( \
		rosidl_typesupport_microxrcedds_c, PkgName, SrvSubfolder, SrvName)()

#endif /* CONFIG_LEXXHARD_MICROROS_DIRECT_TYPESUPPORT */

#endif /* LEXXHARD_MICROROS_TYPESUPPORT_H */
