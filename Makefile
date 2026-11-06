# Copyright (c) 2024, LexxPluss Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VERSION:=$(shell git describe --tags HEAD)
RUNNER:=$(if $(IN_HOST), $(), docker compose run --rm zephyrbuilder)

.PHONY: all
all: 
	$(MAKE) clean
	$(MAKE) firmware_initial
	$(MAKE) clean
	$(MAKE) firmware_tug_initial
	$(MAKE) clean
	$(MAKE) firmware_interlock_initial

.PHONY: clean
clean:
	rm -rf build-mcuboot build

.PHONY: distclean
distclean: clean
	rm -rf build-mcuboot build bootloader modules ros_msgs tools zephyr out .west

.PHONY: build
build: docker-compose.yml Dockerfile
	docker compose build

# This branch is the Zephyr 4.1 + micro-ROS spike: setup and firmware are replaced by the spike scripts.
# The other targets (firmware_tug, firmware_interlock, bootloader, *_initial, all) are the mainline ones
# and are not usable on this branch.
.PHONY: setup
setup:
	spike_tools/build/setup_workspace.sh

.PHONY: update
update:
	$(RUNNER) west update

.PHONY: bootloader
bootloader:
	$(RUNNER) bash -c "west zephyr-export && west build -b lexxpluss_mb02 bootloader/mcuboot/boot/zephyr -d build-mcuboot"
	mv build-mcuboot/zephyr/zephyr.bin out/zephyr.bin

.PHONY: firmware
firmware:
	spike_tools/build/u12f_build.sh

.PHONY: firmware_tug
firmware_tug: 
	$(RUNNER) bash -c "west zephyr-export && west build -b lexxpluss_mb02 lexxpluss_apps -- -DVERSION=$(VERSION) -DENABLE_TUG=1"
	mv build/zephyr/zephyr.signed.bin out/zephyr_tug.signed.bin
	mv build/zephyr/zephyr.signed.confirmed.bin out/zephyr_tug.signed.confirmed.bin

.PHONY: firmware_interlock
firmware_interlock:
	$(RUNNER) bash -c "west zephyr-export && west build -b lexxpluss_mb02 lexxpluss_apps -- -DVERSION=$(VERSION) -DENABLE_INTERLOCK=1"
	mv build/zephyr/zephyr.signed.bin out/zephyr_interlock.signed.bin
	mv build/zephyr/zephyr.signed.confirmed.bin out/zephyr_interlock.signed.confirmed.bin

.PHONY: firmware_initial
firmware_initial: 
	$(MAKE) bootloader
	$(MAKE) firmware
	dd if=/dev/zero bs=1k count=256 | tr "\000" "\377" > out/bl_with_ff.bin
	dd if=out/zephyr.bin of=out/bl_with_ff.bin conv=notrunc
	cat out/bl_with_ff.bin out/zephyr.signed.bin > out/firmware.bin

.PHONY: firmware_tug_initial
firmware_tug_initial: 
	$(MAKE) bootloader
	$(MAKE) firmware_tug
	dd if=/dev/zero bs=1k count=256 | tr "\000" "\377" > out/bl_with_ff.bin
	dd if=out/zephyr.bin of=out/bl_with_ff.bin conv=notrunc
	cat out/bl_with_ff.bin out/zephyr_tug.signed.bin > out/firmware_tug.bin

.PHONY: firmware_interlock_initial
firmware_interlock_initial:
	$(MAKE) bootloader
	$(MAKE) firmware_interlock
	dd if=/dev/zero bs=1k count=256 | tr "\000" "\377" > out/bl_with_ff.bin
	dd if=out/zephyr.bin of=out/bl_with_ff.bin conv=notrunc
	cat out/bl_with_ff.bin out/zephyr_interlock.signed.bin > out/firmware_interlock.bin
