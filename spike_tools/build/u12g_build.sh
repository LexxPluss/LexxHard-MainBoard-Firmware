#!/usr/bin/env bash
set -u
# U12 unit g: agent wait and reconnect (FW_UNIT=h, real slot, NODES=2) with the checks E1, E6, E8, E9, E10, E11.
# Usage: spike_tools/build/u12g_build.sh (run from anywhere, as ! <abs path of the worktree>/spike_tools/build/u12g_build.sh)
. "$(dirname "$(readlink -f "$0")")/u12_common.sh"
u12_run g
exit $?
