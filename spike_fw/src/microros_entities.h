/*
 * micro-ROS entity table (X-macro). Deliberately has no include guard: it is included several times.
 *
 * Line format:
 *   MICROROS_ENTITY(<PUB|SUB|SRV>, "<name>", <package>, <Type>, <UART6|UART2>, RELIABLE)
 *
 * The including file defines MICROROS_ENTITY before the include and undefines it after.
 */

MICROROS_ENTITY(PUB, "/body_control/encoder_count", std_msgs, Int32MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/body_control/shelf_connection", std_msgs, Float32MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/body_control/linear_actuator_current", std_msgs, Float32MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/battery", lexxauto_msgs, Battery, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/imu", lexxauto_msgs, Imu, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/control/emergency_stop_at_connected_robot", std_msgs, Bool, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/pgv", lexxauto_msgs, PositionGuideVision, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/downward", std_msgs, Float64MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/ultrasonic", std_msgs, Float64MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/towing_unit", std_msgs, UInt8MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/lexxhard/dfu_response", std_msgs, UInt16MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/fan", std_msgs, UInt8MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/bumper", std_msgs, ByteMultiArray, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/emergency_switch", std_msgs, Bool, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/body_control/charge_status", std_msgs, Byte, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/sensor_set/temperature", lexxauto_msgs, BoardTemperatures, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/body_control/power_state", std_msgs, Byte, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/body_control/charge_heartbeat_delay", std_msgs, UInt8, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/body_control/charge_connector_voltage", std_msgs, Float32, UART6, RELIABLE)
MICROROS_ENTITY(PUB, "/diagnostics", diagnostic_msgs, DiagnosticArray, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/body_control/linear_actuator", lexxauto_msgs, LinearActuatorControlArray, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/control/emergency_stop_at_amr", std_msgs, Bool, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/sensor_set/pgv_dir", std_msgs, UInt8, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/lexxhard/dfu_data", std_msgs, UInt8MultiArray, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/body_control/led", std_msgs, String, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/body_control/led_direct", lexxauto_msgs, Led, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/control/request_emergency_stop", std_msgs, Bool, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/control/request_power_off", std_msgs, Bool, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/control/request_lockdown", std_msgs, Bool, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/lexxhard/setup", std_msgs, String, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/lexxhard/mainboard_messenger_heartbeat", std_msgs, Bool, UART6, RELIABLE)
MICROROS_ENTITY(SUB, "/control/towing_unit_power_on", std_msgs, UInt8, UART6, RELIABLE)
MICROROS_ENTITY(SRV, "/body_control/linear_actuator_location", lexxauto_msgs, LinearActuatorLocation, UART2, RELIABLE)
MICROROS_ENTITY(SRV, "/body_control/init_linear_actuator", lexxauto_msgs, InitLinearActuator, UART2, RELIABLE)
MICROROS_ENTITY(SRV, "/control/auto_charge_request/enable", std_srvs, SetBool, UART2, RELIABLE)
