# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../spike_minimal)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spike_fw)

# FW_UNIT selects the cumulative unit (a..g) up to which sources are compiled.
if(NOT DEFINED FW_UNIT OR NOT FW_UNIT MATCHES "^[a-g]$")
    message(FATAL_ERROR "FW_UNIT must be set to a letter a..g (e.g. -DFW_UNIT=a)")
endif()

# Maps the unit letter to an index (a=1 .. g=7).
function(fw_unit_index letter out_var)
    string(FIND "abcdefg" "${letter}" pos)
    math(EXPR idx "${pos} + 1")
    set(${out_var} ${idx} PARENT_SCOPE)
endfunction()

# Adds sources when FW_UNIT is at or after the given unit letter.
function(fw_unit_sources letter)
    fw_unit_index("${FW_UNIT}" current)
    fw_unit_index("${letter}" required)
    if(NOT current LESS required)
        target_sources(app PRIVATE ${ARGN})
    endif()
endfunction()

# Defines FW_HAS_<NAME>=1 for the named controllers when FW_UNIT is at or after
# the given unit letter, so that main.cpp can skip controllers not built yet.
function(fw_unit_defines letter)
    fw_unit_index("${FW_UNIT}" current)
    fw_unit_index("${letter}" required)
    if(NOT current LESS required)
        foreach(name ${ARGN})
            target_compile_definitions(app PRIVATE FW_HAS_${name}=1)
        endforeach()
    endif()
endfunction()

# Cumulative source table. Later units extend this table (b: in-house driver
# module path; g: micro-ROS).
fw_unit_sources(a src/main.cpp)
fw_unit_sources(c src/can_controller.cpp src/interlock_controller.cpp)
fw_unit_sources(d src/actuator_controller.cpp src/adc_reader.cpp
                  src/pgv_controller.cpp src/towing_unit_controller.cpp
                  src/misc_controller.cpp)
fw_unit_sources(e src/firmware_updater.cpp src/sdlog_controller.cpp
                  src/log_controller.cpp src/runaway_detector.cpp
                  src/tof_controller.cpp)
fw_unit_sources(f src/imu_controller.cpp src/uss_controller.cpp
                  src/led_controller.cpp)

fw_unit_defines(c CAN INTERLOCK)
fw_unit_defines(d ACTUATOR ADC PGV TOWING MISC)
fw_unit_defines(e UPDATER SDLOG LOG RUNAWAY TOF)
fw_unit_defines(f IMU USS LED)

if(ENABLE_INTERLOCK)
    add_definitions(-DENABLE_INTERLOCK)
endif()

if(ENABLE_TUG)
    add_definitions(-DENABLE_TUG)
endif()

if(VERSION)
    add_definitions(-DVERSION=${VERSION})
endif()
