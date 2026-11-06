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
