#!/usr/bin/env bash
set -u
# U12 unit d: build spike_fw (FW_UNIT=h) and run the unit checks. Usage: ! spike_tools/build/u12d_build.sh (run from anywhere, as ! <abs path of the worktree>/spike_tools/build/u12d_build.sh)
. "$(dirname "$(readlink -f "$0")")/u12_common.sh"
u12_run d
exit $?
