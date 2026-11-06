# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../spike_minimal)
# From unit b on, the in-house driver module is part of the build. The module
# list must be known before find_package(Zephyr), so the unit index is derived here.
if(DEFINED FW_UNIT AND FW_UNIT MATCHES "^[a-h]$")
    string(FIND "abcdefgh" "${FW_UNIT}" early_unit_pos)
    if(early_unit_pos GREATER 0)
        list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/modules/lexxpluss_drivers)
        list(APPEND EXTRA_CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/prj_sensors.conf)
    endif()
    # Unit g adds the micro-ROS module (same early-index technique).
    if(early_unit_pos GREATER 5)
        list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../micro_ros_zephyr_module/modules/libmicroros)
        # MEASUREMENT ONLY: enlarged code partition (see the overlay); applied after the board overlay.
        list(APPEND EXTRA_DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/boards/u9g_measure_flash_region.overlay)
    endif()
endif()
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spike_fw)

# Under LTO the compiler emits the 64-bit atomic libcalls during link-time codegen, after the
# linker has chosen the libmicroros.a members, so librcutils-atomic_64bits.c.obj is not pulled
# in. Force its symbols (all four defined in rcutils atomic_64bits.c) to be undefined at link.
if(CONFIG_LTO)
    zephyr_ld_options(
        -Wl,-u,__atomic_load_8
        -Wl,-u,__atomic_store_8
        -Wl,-u,__atomic_exchange_8
        -Wl,-u,__atomic_fetch_add_8
    )
endif()

# FW_UNIT selects the cumulative unit (a..h) up to which sources are compiled.
if(NOT DEFINED FW_UNIT OR NOT FW_UNIT MATCHES "^[a-h]$")
    message(FATAL_ERROR "FW_UNIT must be set to a letter a..h (e.g. -DFW_UNIT=a)")
endif()

# Maps the unit letter to an index (a=1 .. g=7).
function(fw_unit_index letter out_var)
    string(FIND "abcdefgh" "${letter}" pos)
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
fw_unit_sources(c src/can_controller.cpp src/interlock_controller.cpp
                  src/u9_stubs.cpp)
fw_unit_sources(d src/actuator_controller.cpp src/adc_reader.cpp
                  src/pgv_controller.cpp src/towing_unit_controller.cpp
                  src/misc_controller.cpp)
fw_unit_sources(e src/firmware_updater.cpp src/sdlog_controller.cpp
                  src/log_controller.cpp src/runaway_detector.cpp
                  src/tof_controller.cpp)
fw_unit_sources(f src/imu_controller.cpp src/uss_controller.cpp
                  src/led_controller.cpp)
# The size-spike sample is compiled for unit g only; unit h replaces it by the real session.
if(FW_UNIT STREQUAL "g")
    target_sources(app PRIVATE src/microros_types.c)
endif()
fw_unit_sources(g src/microros_transport_uart.c)
fw_unit_sources(h src/microros_session.c)

fw_unit_defines(c CAN INTERLOCK)
fw_unit_defines(d ACTUATOR ADC PGV TOWING MISC)
fw_unit_defines(e UPDATER SDLOG LOG RUNAWAY TOF)
fw_unit_defines(f IMU USS LED)
fw_unit_defines(g MICROROS)
fw_unit_defines(h MICROROS_REAL)

if(ENABLE_INTERLOCK)
    add_definitions(-DENABLE_INTERLOCK)
endif()

if(ENABLE_TUG)
    add_definitions(-DENABLE_TUG)
endif()

if(VERSION)
    add_definitions(-DVERSION=${VERSION})
endif()
