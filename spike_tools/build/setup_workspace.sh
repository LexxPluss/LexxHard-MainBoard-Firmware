#!/usr/bin/env bash
# Rebuild the west workspace of this worktree on a fresh PC (or after .west, zephyr, bootloader, modules,
# ros_msgs, tools, out, build-fw-* were deleted), so that u10z_build.sh and u12*_build.sh find what they assume.
# Steps (in the docker image): west init -l lexxpluss_apps; west update; west config --global zephyr.base-prefer
# configfile; mkdir -p out; then the micro-ROS ROS 2 sources via the module's own make rule (git clones, network).
# Idempotent: the west step is skipped when .west exists, the micro-ROS fetch is skipped when micro_ros_src/src
# exists, and the docker image is pulled when absent. Refuses (never deletes anything) when micro_ros_src exists
# without src.
# Without SETUP_WORKER it runs the worker under loop_run.sh (stdout = loop_run.sh lines only);
# with SETUP_WORKER=1 it is the worker: all output goes to the log through loop_run.sh.
set -u

SETUP_HERE="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
SETUP_LOOP_RUN="$SETUP_HERE/loop_run.sh"
SETUP_WT="$(git -C "$SETUP_HERE" rev-parse --show-toplevel)"
SETUP_BRANCH="feat/zephyr-4.1-microros-spike"
SETUP_IMG=zephyrprojectrtos/zephyr-build:v0.27.4
SETUP_MK_REL=micro_ros_zephyr_module/modules/libmicroros

setup_provenance() {
  local m=$SETUP_WT/$SETUP_MK_REL r d
  echo "== provenance (floating branches: zephyr tag v4.1.0, ros_msgs main, micro_ros_zephyr_module jazzy, ROS 2 jazzy/ros2)"
  echo "date: $(date -Is)"
  echo "image: $SETUP_IMG id $(docker image inspect "$SETUP_IMG" --format '{{.Id}}' 2>&1)"
  echo "worktree HEAD: $(git -C "$SETUP_WT" rev-parse HEAD 2>&1)"
  echo "west.yml md5: $(md5sum "$SETUP_WT/lexxpluss_apps/west.yml" | cut -d' ' -f1)"
  for d in zephyr micro_ros_zephyr_module ros_msgs; do
    echo "$d HEAD: $(git -C "$SETUP_WT/$d" rev-parse HEAD 2>&1) $(git -C "$SETUP_WT/$d" remote get-url origin 2>&1)"
  done
  echo "ROS 2 source repositories in micro_ros_src/src (HEAD, remote url):"
  find "$m/micro_ros_src/src" -maxdepth 2 -name .git 2>/dev/null | sort | while read -r g; do
    r=$(dirname "$g")
    echo "  ${r#"$m"/micro_ros_src/src/} $(git -C "$r" rev-parse HEAD 2>&1) $(git -C "$r" remote get-url origin 2>&1)"
  done
}

setup_worker() {
  local m=$SETUP_WT/$SETUP_MK_REL
  local mk=$m/libmicroros.mk
  echo "setup worker: stamp=$SETUP_STAMP $(date -Is) worktree=$SETUP_WT"
  if ! docker image inspect "$SETUP_IMG" >/dev/null 2>&1; then
    echo "== docker image absent, pulling $SETUP_IMG (network)"
    docker pull "$SETUP_IMG" 2>&1
    docker image inspect "$SETUP_IMG" >/dev/null 2>&1 || { echo "FAIL image absent after docker pull: $SETUP_IMG"; return 1; }
  fi
  [ -f "$SETUP_WT/lexxpluss_apps/west.yml" ] || { echo "FAIL lexxpluss_apps/west.yml absent"; return 1; }

  local rc
  if [ -e "$SETUP_WT/.west" ]; then
    echo "west workspace present, skipped"
  else
    echo "== west init, west update, west config, mkdir out (in $SETUP_IMG)"
    docker run --rm -v "$SETUP_WT":/workdir -w /workdir -e ZEPHYR_TOOLCHAIN_VARIANT=zephyr "$SETUP_IMG" bash -c '
      set -e
      west init -l lexxpluss_apps
      west update
      west config --global zephyr.base-prefer configfile
      mkdir -p out' 2>&1 | grep -v -E "Openbox|Xlib|VNC|PORT="
    rc=${PIPESTATUS[0]}
    echo "WEST_RC=$rc"
    [ "$rc" -eq 0 ] || return "$rc"
  fi
  [ -f "$mk" ] || { echo "FAIL $mk absent after west update"; return 1; }

  if [ -d "$m/micro_ros_src/src" ]; then
    echo "== micro_ros_src/src already present after west update; not fetching"
  else
    if [ -e "$m/micro_ros_src" ]; then
      echo "FAIL $m/micro_ros_src exists without src; the module rule would remove it. Remove it by hand and rerun the make step (nothing was deleted)"
      return 1
    fi
    echo "== fetch ROS 2 sources with the module rule (cwd = libmicroros dir; needs network)"
    docker run --rm -v "$SETUP_WT":/workdir -w "/workdir/$SETUP_MK_REL" "$SETUP_IMG" \
      make -f libmicroros.mk "COMPONENT_PATH=/workdir/$SETUP_MK_REL" "/workdir/$SETUP_MK_REL/micro_ros_src/src" 2>&1 \
      | grep -v -E "Openbox|Xlib|VNC|PORT="
    rc=${PIPESTATUS[0]}
    echo "MAKE_RC=$rc"
    # The rule is one ';' chain, so its exit status is the last command only: count the clones to catch a failed one.
    local want have
    want=$(awk '/^\$\(COMPONENT_PATH\)\/micro_ros_src\/src:/{f=1; next} f&&/^$/{exit} f&&/git clone/{n++} END{print n+0}' "$mk")
    have=$(find "$m/micro_ros_src/src" -maxdepth 2 -name .git 2>/dev/null | wc -l)
    echo "CHECK clones expected=$want present=$have"
    if [ "$want" -eq 0 ] || [ "$have" -ne "$want" ] || [ "$rc" -ne 0 ]; then
      echo "FAIL micro_ros_src/src incomplete (remove $m/micro_ros_src by hand before retrying)"
      setup_provenance
      return 1
    fi
  fi
  setup_provenance
  return 0
}

if [ "${SETUP_WORKER:-}" = 1 ]; then
  setup_worker
  exit $?
fi

SETUP_STAMP=$(date +%Y%m%d_%H%M%S)
SETUP_LOG=${SPIKE_LOG_DIR:-$HOME/log}/spike_setup_${SETUP_STAMP}.log
mkdir -p "${SPIKE_LOG_DIR:-$HOME/log}"
: > "$SETUP_LOG"
"$SETUP_LOOP_RUN" --kind build --log "$SETUP_LOG" --repo "$SETUP_WT" --branch "$SETUP_BRANCH" -- \
  env SETUP_WORKER=1 SETUP_STAMP="$SETUP_STAMP" bash "$(readlink -f "${BASH_SOURCE[0]}")"
exit $?
