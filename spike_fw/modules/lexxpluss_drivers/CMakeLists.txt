# SPDX-License-Identifier: BSD-2-Clause
# Glue for the unmodified LexxPluss in-house drivers (copied from the 2.6 fork).
# Each driver directory calls zephyr_library() itself and gates its sources
# with its own CONFIG_ symbol, so the directories are added unconditionally.
add_subdirectory(drivers/sensor/maxbotix)
add_subdirectory(drivers/sensor/adis16470)
