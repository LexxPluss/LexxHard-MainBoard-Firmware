#!/usr/bin/env bash
set -u
# U10z: build spike_fw (FW_UNIT=g) with the U10 unit-z fragment. Usage: ! spike_tools/build/u10z_build.sh (run from anywhere, as ! <abs path of the worktree>/spike_tools/build/u10z_build.sh)
. "$(dirname "$(readlink -f "$0")")/u10_common.sh"
u10_run z
exit $?
