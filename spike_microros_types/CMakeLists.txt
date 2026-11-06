# SPDX-License-Identifier: BSD-2-Clause
cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_LIST_DIR}/../spike_minimal)
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../micro_ros_zephyr_module/modules/libmicroros)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(spike_microros_types)
target_sources(app PRIVATE src/main.c)
