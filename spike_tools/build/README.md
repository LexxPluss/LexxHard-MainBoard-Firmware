# spike_tools/build

This branch is special: it carries its own build recipe, so another PC can build the same spike images.

## Prerequisites

- Docker image `zephyrprojectrtos/zephyr-build:v0.27.4` is present.
- The west workspace is populated in this worktree: `zephyr`, `modules`, the `bootloader/mcuboot` key,
  and `micro_ros_zephyr_module/modules/libmicroros` with `micro_ros_src`.

## Scripts

| Script | Builds |
|---|---|
| `u10z_build.sh` | unit g baseline (FW_UNIT=g, U10 fragment) |
| `u12a1_build.sh`, `u12a2_build.sh`, `u12b_build.sh`..`u12e_build.sh` | unit h steps (FW_UNIT=h) plus the unit checks |
| `u12f_build.sh` | on-board image (FW_UNIT=h, real slot, NODES=2); outputs `zephyr.signed.bin` and `zephyr.signed.confirmed.bin` in `${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u12_f_<stamp>/` |

Support files: `u10_common.sh`, `u12_common.sh`, `u12_check_entities.sh`, `u12_entities.txt`, `loop_run.sh`.

## Run

    ! <abs path of the worktree>/spike_tools/build/<name>.sh

The worktree is derived from the script location. The branch guard requires `feat/zephyr-4.1-microros-spike`.

## Logs

Logs and evidence go to `${SPIKE_LOG_DIR:-$HOME/log}` (created with `mkdir -p`).

## Notes

- The scripts patch `libmicroros.mk` temporarily and restore it afterwards.
- Compare results by the CHECK lines in the log.
