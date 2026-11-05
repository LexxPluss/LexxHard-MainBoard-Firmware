cmake_minimum_required(VERSION 3.8)
project(lexxauto_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

# the 9 definitions below are the ones the MainBoard firmware uses; the files are copied unchanged from
# LexxAuto src/lexxauto_msgs/ros2 (the ROS 2 definitions of the migration branch)
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/Battery.msg"
  "msg/BoardTemperatures.msg"
  "msg/Imu.msg"
  "msg/Led.msg"
  "msg/LinearActuatorControl.msg"
  "msg/LinearActuatorControlArray.msg"
  "msg/PositionGuideVision.msg"
  "srv/InitLinearActuator.srv"
  "srv/LinearActuatorLocation.srv"
  DEPENDENCIES geometry_msgs std_msgs sensor_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_package()
