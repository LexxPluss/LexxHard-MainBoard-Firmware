# LexxPluss Main Board Software

[![CI](https://github.com/LexxPluss/LexxHard-MainBoard-Firmware/actions/workflows/main.yml/badge.svg)](https://github.com/LexxPluss/LexxHard-MainBoard-Firmware/actions/workflows/main.yml)
[![release](https://github.com/LexxPluss/LexxHard-MainBoard-Firmware/actions/workflows/release.yml/badge.svg)](https://github.com/LexxPluss/LexxHard-MainBoard-Firmware/actions/workflows/release.yml)

## For Docker (Ubuntu)

## Install dependencies 

```bash
$ mkdir -p $HOME/zephyrproject/
$ cd $HOME/zephyrproject/
$ git clone https://github.com/LexxPluss/LexxHard-MainBoard-Firmware
```
## Setup Zephyr

```bash
$ export ZEPHYR_BASE=/workdir/LexxHard-MainBoard-Firmware/zephyr
$ cd /workdir/LexxHard-MainBoard-Firmware
$ make setup
```
## Build
### Build bootloader (MCUboot)

```bash
$ make bootloader
```
### Build firmware

```bash
$ make firmware
```
```bash
$ make firmware_initial
```

### Build firmware ( enable interlock )

```bash
$ make firmware_interlock
```

### Build firmware ( enable tug )

```bash
$ make firmware_tug
```

---
## For macOS

## Install dependencies　

Prepare a development environment referring to
https://docs.zephyrproject.org/2.7.0/getting_started/

### Install Homebrew

```bash
$ /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### Install Utils

```bash
$ brew install cmake ninja gperf python3 ccache qemu dtc
$ pip3 install -U west
```

### Setup Zephyr

```bash
$ west init -l lexxpluss_apps
$ west update
$ west zephyr-export
$ pip3 install -r zephyr/scripts/requirements.txt
```

### Install Toolchain

Install Toolchain and set environment variables referring to
https://docs.zephyrproject.org/2.7.0/getting_started/toolchain_3rd_party_x_compilers.html

```bash
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/Applications/ARM
```

## Build

### Build bootloader (MCUboot)

```bash
$ west build -b lexxpluss_mb02 bootloader/mcuboot/boot/zephyr -d build-mcuboot
```

### Build firmware

```bash
$ west build -p auto -b lexxpluss_mb02 lexxpluss_apps
```

### Build firmware ( enable interlock )

```bash
$ west build -p auto -b lexxpluss_mb02 lexxpluss_apps -- -DENABLE_INTERLOCK=1
```

### Build firmware ( enable tug )

```bash
$ west build -p auto -b lexxpluss_mb02 lexxpluss_apps -- -DENABLE_TUG=1
```

---
## Program of the built firmware

### First time program

Program the bootloader and signed firmware after erasing the entire Flash ROM.
Output binaries are in following output directories.

#### mac
* <bootloader output dir>: `out`
* <firmware output dir>: `out`


#### linux
* <bootloader output dir>: `build-mcuboot/zephyr`
* <firmware output dir>: `build/zephyr`

```bash
$ brew install stlink
$ st-flash --reset --connect-under-reset erase
$ st-flash --reset --connect-under-reset write <bootloader output dir>/zephyr.bin 0x8000000
$ st-flash --reset --connect-under-reset write <firmware output dir>/zephyr.signed.bin 0x8040000
```

### Update

Program the firmware for update to the update area.

```bash
$ st-flash --reset --connect-under-reset write <firmware output dir>/zephyr.signed.confirmed.bin 0x8080000
```

## Program of the released firmware

```bash
$ st-flash --reset --connect-under-reset erase
$ st-flash --reset --connect-under-reset write LexxHard-MainBoard-Firmware-Initial-v?.?.? 0x8000000
```

## Update via ROS

Use [LexxPluss/LexxHard-MainBoard-Updator](https://github.com/LexxPluss/LexxHard-MainBoard-Updator.git).

The firmware is automatically updated when the robot is turned on again after executing the following command.
```bash
$ rosrun mainboard_updator mainboard_updator LexxHard-MainBoard-Firmware-Update-v?.?.?.bin
```

## License

Copyright (c) 2022, LexxPluss Inc. Released under the [BSD License](LICENSE).
