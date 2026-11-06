#!/usr/bin/env bash
set -u
# U12 unit f: on-board image (FW_UNIT=h, real slot, NODES=2) and the unit checks. Usage: ! spike_tools/build/u12f_build.sh (run from anywhere, as ! <abs path of the worktree>/spike_tools/build/u12f_build.sh)
. "$(dirname "$(readlink -f "$0")")/u12_common.sh"
u12_run f
exit $?
