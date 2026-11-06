# spike_tools/build

This branch is special: it carries its own build recipe, so another PC can build the same spike images.

## Usage

Clone this branch (it must exist on that remote; it is not pushed yet), then run from the worktree root:

    git clone --branch feat/zephyr-4.1-microros-spike <repo> <dir>
    cd <dir>
    make setup
    make firmware

Both commands are run by the user and use docker (network access to github.com and Docker Hub is needed).
`make setup` runs `spike_tools/build/setup_workspace.sh`; `make firmware` runs `spike_tools/build/u12f_build.sh`.
Outputs: `out/zephyr.signed.bin` and `out/zephyr.signed.confirmed.bin` (the log prints an `IMAGES` line with their
md5), and the evidence dir `${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u12_f_<stamp>/`. The other Makefile targets are
mainline ones and are not usable on this branch.

## Disk and time

Not measured yet (disk use of the workspace and docker image, and build time).

## Scripts

| Script | Builds |
|---|---|
| `u10z_build.sh` | unit g baseline (FW_UNIT=g, U10 fragment) |
| `u12a1_build.sh`, `u12a2_build.sh`, `u12b_build.sh`..`u12e_build.sh` | unit h steps (FW_UNIT=h) plus the unit checks |
| `u12f_build.sh` | on-board image (FW_UNIT=h, real slot, NODES=2); outputs `zephyr.signed.bin` and `zephyr.signed.confirmed.bin` in `${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u12_f_<stamp>/` |

Support files: `u10_common.sh`, `u12_common.sh`, `u12_check_entities.sh`, `u12_entities.txt`, `loop_run.sh`.

## Setting up a fresh workspace

Nothing to do by hand: `u10z_build.sh` and every `u12*_build.sh` check `.west`, `zephyr`,
`bootloader/mcuboot/root-rsa-2048.pem`, `micro_ros_src/src` and the docker image first. If any is missing they run
`setup_workspace.sh`, print its three lines, and stop with its exit code when it fails. When all are present
nothing extra runs, so repeated builds are unchanged.

`setup_workspace.sh` can still be run alone:

    ! <abs path of the worktree>/spike_tools/build/setup_workspace.sh

It is idempotent and never deletes anything: it pulls the docker image `zephyrprojectrtos/zephyr-build:v0.27.4` when
absent, skips the west step (log `west workspace present, skipped`) when `.west` exists, and otherwise runs
`west init -l lexxpluss_apps`, `west update`, `west config --global zephyr.base-prefer configfile` and `mkdir -p out`
in the image. It fetches `micro_ros_src/src` with the module's own make rule only when it is absent (and refuses when
`micro_ros_src` exists without `src`). The log is `${SPIKE_LOG_DIR:-$HOME/log}/spike_setup_<stamp>.log`; its end lists
the HEAD and remote URL of `zephyr`, `micro_ros_zephyr_module`, `ros_msgs` and every repository in `micro_ros_src/src`.

Note: the manifest follows floating branches (`ros_msgs` main, `micro_ros_zephyr_module` jazzy, and jazzy/ros2 for the
ROS 2 sources). If they moved since the earlier builds, the result and the image md5 can differ; compare the
provenance lines in the log with the earlier evidence.

## Run

    ! <abs path of the worktree>/spike_tools/build/<name>.sh

The worktree is derived from the script location. The branch guard requires `feat/zephyr-4.1-microros-spike`.

## Logs

Logs and evidence go to `${SPIKE_LOG_DIR:-$HOME/log}` (created with `mkdir -p`).

## Notes

- The scripts patch `libmicroros.mk` temporarily and restore it afterwards.
- Compare results by the CHECK lines in the log.
