# spike_tools/build

This branch is special: it carries its own build recipe, so another PC can build the same spike images.

## Usage

Clone this branch (it must exist on that remote; it is not pushed yet), then run from the worktree root:

    git clone --branch feat/zephyr-4.1-microros-spike <repo> <dir>
    cd <dir>
    make setup
    make firmware

Both commands are run by the user and use docker (network access to github.com and Docker Hub is needed).
`make setup` runs `spike_tools/build/setup_workspace.sh`; `make firmware` runs `spike_tools/build/u12g_build.sh` (the f build plus the checks E10 and E11).
Outputs: `out/zephyr.signed.bin` and `out/zephyr.signed.confirmed.bin` (the log prints an `IMAGES` line with their
md5), and the evidence dir `${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u12_f_<stamp>/`. The other Makefile targets are
mainline ones and are not usable on this branch.

## Disk and time

One measurement on one machine (mm-laptop, fast network, 2026-11-06); other machines will differ.

- `make setup`: about 4 minutes (west update of zephyr v4.1.0, modules, ros_msgs, micro_ros_zephyr_module, and 22 clones of the micro-ROS ROS 2 sources).
- `make firmware`: about 2 minutes after setup.
- Disk of the worktree after setup and one firmware build: 3.1 GB (zephyr 1.4 GB, modules 1.3 GB, micro_ros_zephyr_module 471 MB, build directory 48 MB).
- Docker image `zephyrprojectrtos/zephyr-build:v0.27.4`: 16.2 GB (pulled by setup when absent).
- The times are for a machine that already had the docker image.

## Comparing results

The CHECK lines (E1, E6, E8, E9) are the pass criteria. The md5 of the whole `zephyr.signed.bin` is NOT a
reproducibility criterion: the RSA signature in its last 256 bytes differs from build to build (measured: two builds
of the same source differ only in bytes 240133 to 240388 of 240388). To compare two builds, compare the file without
its signature part, e.g. `cmp -n 240132 <a> <b>` (the length is the payload size of that build, `signed_image_bytes`
minus 256, valid while the size is equal).

In a fresh clone the micro-ROS sources came out at the same commits as the earlier builds (22 ROS 2 repositories and
micro_ros_zephyr_module 87dbe3a).

## Scripts

| Script | Builds |
|---|---|
| `u10z_build.sh` | unit g baseline (FW_UNIT=g, U10 fragment) |
| `u12a1_build.sh`, `u12a2_build.sh`, `u12b_build.sh`..`u12e_build.sh` | unit h steps (FW_UNIT=h) plus the unit checks |
| `u12f_build.sh` | on-board image (FW_UNIT=h, real slot, NODES=2); outputs `zephyr.signed.bin` and `zephyr.signed.confirmed.bin` in `${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u12_f_<stamp>/` |
| `u12g_build.sh` | agent wait and reconnect (FW_UNIT=h, real slot, NODES=2): the f build plus the checks E10 (no empty loop) and E11 (fini defined and called); this is what `make firmware` runs; the images go to `out/` and the evidence directory |

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
