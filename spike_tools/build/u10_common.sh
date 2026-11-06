#!/usr/bin/env bash
# Shared pieces of the U10 size-reduction build scripts (sourced by u10z/a/b/c/e_build.sh, not run directly).
# Every unit builds the same firmware as U9 unit g (FW_UNIT=g) plus one extra Kconfig fragment per unit.
# u10_run <unit>: without U10_WORKER it runs the worker under loop_run.sh (stdout = loop_run.sh lines only);
# with U10_WORKER=1 it is the worker: all output goes to the log through loop_run.sh.
U10_HERE="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
U10_LOOP_RUN="$U10_HERE/loop_run.sh"
U10_WT="$(git -C "$U10_HERE" rev-parse --show-toplevel)"
U10_BRANCH="feat/zephyr-4.1-microros-spike"
U10_IMG=zephyrprojectrtos/zephyr-build:v0.27.4
U10_SCRIPTS="$U10_HERE"
U10_DESC_OPT="-DROSIDL_GENERATOR_C_DISABLE_TYPE_DESCRIPTION_CODEGEN=ON"
U10_OVERLAY=spike_fw/variants/microros_nano_nodesc.conf
U10_REF=spike_fw/variants/u10_baseline.txt
U10_B0_FLASH=275446   # baseline B0 (measured, U9g): size -B text+data
U10_B0_RAM=253172     # baseline B0 (measured, U9g): size -B data+bss

u10_fragment() {  # u10_fragment <unit>: prints the fragment file name (empty for z)
  case "$1" in
    z) echo "" ;;
    a) echo "u10a_lto.conf" ;;
    b) echo "u10b_noexec.conf" ;;
    c) echo "u10c_direct_ts.conf" ;;
    e) echo "u10e_combined.conf" ;;
    *) echo "?" ;;
  esac
}

u10_ensure_workspace() {
  # Fresh clone: run setup_workspace.sh first when anything the build assumes is missing; no-op otherwise.
  local m=$U10_WT/micro_ros_zephyr_module/modules/libmicroros
  if [ -e "$U10_WT/.west" ] && [ -e "$U10_WT/zephyr" ] && [ -f "$U10_WT/bootloader/mcuboot/root-rsa-2048.pem" ] \
     && [ -d "$m/micro_ros_src/src" ] && docker image inspect "$U10_IMG" >/dev/null 2>&1; then
    return 0
  fi
  echo "workspace incomplete, running setup_workspace.sh"
  local out rc
  out=$("$U10_HERE/setup_workspace.sh")
  rc=$?
  echo "$out"
  return $rc
}

u10_run() {
  local unit=$1 self
  self=$(readlink -f "${BASH_SOURCE[1]}")
  if [ "${U10_WORKER:-}" = 1 ]; then
    u10_worker "$unit" "$self"
    return $?
  fi
  local stamp log rc
  u10_ensure_workspace || return $?
  stamp=$(date +%Y%m%d_%H%M%S)
  log=${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u10_${unit}_${stamp}.log
  mkdir -p ${SPIKE_LOG_DIR:-$HOME/log}
  : > "$log"
  "$U10_LOOP_RUN" --kind build --log "$log" --repo "$U10_WT" --branch "$U10_BRANCH" -- \
    env U10_WORKER=1 U10_STAMP="$stamp" U10_LOG="$log" bash "$self"
  rc=$?
  return $rc
}

u10_provenance() {  # u10_provenance <dir> <self> <module-dir> <orig-mk> <fragment-name>
  local d=$1 self=$2 m=$3 orig=$4 frag=$5
  {
    echo "date: $(date -Is)"
    echo "script: $self md5 $(md5sum "$self" | cut -d' ' -f1)"
    echo "common: ${BASH_SOURCE[0]} md5 $(md5sum "${BASH_SOURCE[0]}" | cut -d' ' -f1)"
    echo "image: $U10_IMG id $(docker image inspect "$U10_IMG" --format '{{.Id}}')"
    echo "worktree HEAD: $(git -C "$U10_WT" rev-parse HEAD)  branch: $(git -C "$U10_WT" branch --show-current)"
    echo "worktree status (tracked changes):"; git -C "$U10_WT" status --short | grep -v '^??' || echo "  none"
    echo "west.yml md5: $(md5sum "$U10_WT/lexxpluss_apps/west.yml" | cut -d' ' -f1)"
    echo "module (micro_ros_zephyr_module) HEAD: $(git -C "$U10_WT/micro_ros_zephyr_module" rev-parse HEAD 2>&1)"
    echo "libmicroros.mk md5 (original): $(md5sum "$orig" | cut -d' ' -f1)"
    echo "fragment: ${frag:-none}"
    echo "fragment files md5:"
    (cd "$U10_WT" && md5sum "$U10_OVERLAY" ${frag:+"spike_fw/variants/$frag"} "$U10_REF" 2>&1)
    echo "app files md5 (spike_fw):"; (cd "$U10_WT" && find spike_fw -type f | sort | xargs md5sum)
    echo "extra package md5:"; (cd "$U10_WT/spike_microros_types/extra_packages" && find . -type f | sort | xargs md5sum)
    echo "ROS 2 source repositories in micro_ros_src/src (HEAD each):"
    find "$m/micro_ros_src/src" -maxdepth 2 -name .git | sort | while read -r g; do
      r=$(dirname "$g"); echo "  ${r#$m/micro_ros_src/src/} $(git -C "$r" rev-parse HEAD 2>&1) $(git -C "$r" remote get-url origin 2>&1)"
    done
  } > "$d/provenance.txt" 2>&1
}

u10_members() {  # u10_members <map> <ere>: distinct libmicroros.a members with input sections matching <ere>
  sed -n '/^Linker script and memory map/,$p' "$1" | grep -o 'libmicroros\.a([^)]*)' | sort -u | grep -c -E "$2"
}

u10_worker() {  # u10_worker <unit> <self>
  local unit=$1 self=$2 bd=build-fw-u10$1 rc out frag confs
  local m=$U10_WT/micro_ros_zephyr_module/modules/libmicroros
  local mk=$m/libmicroros.mk
  local ev=${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u10_${unit}_${U10_STAMP}
  out=${U10_LOG%.log}.build.txt
  echo "u10 worker: unit=$unit stamp=$U10_STAMP $(date -Is)"
  frag=$(u10_fragment "$unit")
  [ "$frag" != "?" ] || { echo "BUILD_RC=1"; echo "FAIL unknown unit: $unit"; return 1; }
  docker image inspect "$U10_IMG" >/dev/null 2>&1 || { echo "BUILD_RC=1"; echo "FAIL image absent: $U10_IMG"; return 1; }
  [ -d "$U10_WT/spike_fw" ] || { echo "BUILD_RC=1"; echo "FAIL spike_fw absent"; return 1; }
  [ -f "$U10_WT/$U10_OVERLAY" ] || { echo "BUILD_RC=1"; echo "FAIL overlay absent: $U10_OVERLAY"; return 1; }
  [ -d "$m/micro_ros_src/src" ] || { echo "BUILD_RC=1"; echo "FAIL module work tree absent"; return 1; }
  confs=/workdir/$U10_OVERLAY
  if [ "$unit" = z ]; then
    if [ ! -f "$U10_WT/$U10_REF" ] || ! grep -qx "flash=$U10_B0_FLASH" "$U10_WT/$U10_REF" \
       || ! grep -qx "ram=$U10_B0_RAM" "$U10_WT/$U10_REF"; then
      echo "BUILD_RC=1"; echo "FAIL reference file absent"; return 1
    fi
  else
    [ -f "$U10_WT/spike_fw/variants/$frag" ] || { echo "BUILD_RC=1"; echo "FAIL fragment absent: $frag"; return 1; }
    confs="$confs;/workdir/spike_fw/variants/$frag"
  fi
  if grep -q "DISABLE_TYPE_DESCRIPTION" "$mk"; then echo "BUILD_RC=1"; echo "FAIL libmicroros.mk is already patched (restore it first)"; return 1; fi

  mkdir -p "$ev"
  cp "$mk" "$ev/libmicroros.mk.orig"
  trap "cp '$ev/libmicroros.mk.orig' '$mk'" EXIT
  echo "== module work tree: extra package, std_srvs marker, generated files, mk patch"
  rm -rf "$m/micro_ros_src/src/lexxauto_msgs"
  cp -r "$U10_WT/spike_microros_types/extra_packages/lexxauto_msgs" "$m/micro_ros_src/src/lexxauto_msgs"
  rm -f "$m/micro_ros_src/src/common_interfaces/std_srvs/COLCON_IGNORE"
  rm -rf "$m/micro_ros_src/build" "$m/micro_ros_src/install" "$m/micro_ros_src/log" "$m/micro_ros_src/src/log" \
         "$m/include" "$m/libmicroros.a" "$m/configured_colcon.meta" "$m/zephyr_toolchain.cmake"
  sed -i 's|^\([[:space:]]*\)-DTHIRDPARTY=ON \\$|&\n\1'"$U10_DESC_OPT"' \\|' "$mk"
  diff -u "$ev/libmicroros.mk.orig" "$mk" > "$ev/libmicroros.mk.patch"; cat "$ev/libmicroros.mk.patch"
  grep -q -- "$U10_DESC_OPT" "$mk" || { echo "BUILD_RC=1"; echo "FAIL mk patch not applied"; return 1; }
  u10_provenance "$ev" "$self" "$m" "$ev/libmicroros.mk.orig" "$frag"
  cp "$U10_WT/lexxpluss_apps/west.yml" "$ev/west.yml"

  echo "== build $bd (FW_UNIT=g, EXTRA_CONF_FILE=$confs)"
  docker run --rm -v "$U10_WT":/workdir -w /workdir -e ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
    -e BD="$bd" -e CONFS="$confs" "$U10_IMG" bash -c '
    export ZEPHYR_SDK_INSTALL_DIR=$(ls -d /opt/toolchains/zephyr-sdk-* | head -1); echo "SDK: $ZEPHYR_SDK_INSTALL_DIR"
    west config zephyr.base-prefer configfile
    pip3 install --user catkin_pkg lark-parser "empy==3.3.4" colcon-common-extensions 2>&1 | tail -1
    export PATH=$HOME/.local/bin:$PATH
    pip3 freeze 2>/dev/null | grep -i -E "catkin|lark|empy|colcon" | sed "s/^/PIP: /"
    rm -rf "$BD"
    west build -p always -b lexxpluss_mb02 spike_fw -d "$BD" -- -DFW_UNIT=g "-DEXTRA_CONF_FILE=$CONFS" 2>&1
    rc=$?
    if [ $rc -eq 0 ]; then
      S=$ZEPHYR_SDK_INSTALL_DIR/arm-zephyr-eabi/bin/arm-zephyr-eabi-size
      $S -B "$BD"/zephyr/zephyr.elf; $S -A "$BD"/zephyr/zephyr.elf
    fi
    exit $rc' 2>&1 | grep -v -E "Openbox|Xlib|VNC|PORT=" | tee "$out"
  rc=${PIPESTATUS[0]}
  echo "BUILD_RC=$rc"
  if [ "$rc" -ne 0 ]; then
    echo "first error signature (first 8 matches):"
    grep -n -E "error|Error|fatal|undefined|CMake Error|Kconfig" "$out" | head -n 8
    return "$rc"
  fi
  ls -l "$U10_WT/$bd/zephyr/zephyr.elf"

  echo "== evidence $ev"
  local b=$U10_WT/$bd
  cp "$out" "$ev/build.log"
  grep -E "^PIP:|BUILD_RC=" "$ev/build.log" >> "$ev/provenance.txt" 2>&1
  cp "$b/zephyr/zephyr.elf" "$b/zephyr/zephyr.map" "$b/zephyr/.config" "$ev/"
  cp "$m/libmicroros.a" "$ev/libmicroros.a"
  { echo "zephyr.elf md5 $(md5sum "$ev/zephyr.elf" | cut -d' ' -f1) size $(stat -c %s "$ev/zephyr.elf")"
    echo "libmicroros.a md5 $(md5sum "$ev/libmicroros.a" | cut -d' ' -f1) size $(stat -c %s "$ev/libmicroros.a")"
    echo "overlay md5 $(md5sum "$U10_WT/$U10_OVERLAY" | cut -d' ' -f1)"
    [ -z "$frag" ] || echo "fragment $frag md5 $(md5sum "$U10_WT/spike_fw/variants/$frag" | cut -d' ' -f1)"; } | tee -a "$ev/provenance.txt"
  docker run --rm -v "$ev":/in:ro "$U10_IMG" bash -c '
    S=$(ls -d /opt/toolchains/zephyr-sdk-* | head -1)/arm-zephyr-eabi/bin/arm-zephyr-eabi-size; $S -B /in/zephyr.elf; $S -A /in/zephyr.elf' 2>/dev/null \
    | grep -v -E "Openbox|Xlib|VNC|PORT=" > "$ev/size.txt"
  echo "size -B:"; sed -n '1,2p' "$ev/size.txt"
  python3 "$U10_SCRIPTS/zephyr_map_breakdown.py" "$ev" "$unit=$ev/zephyr.map,$ev/size.txt" > "$ev/report.md" 2> "$ev/report.err" \
    || echo "breakdown FAILED (see $ev/report.err)"

  local text data bss flash ram n_act n_ts n_intro
  read -r text data bss _ < <(sed -n '2p' "$ev/size.txt")
  if ! [[ "${text:-}" =~ ^[0-9]+$ && "${data:-}" =~ ^[0-9]+$ && "${bss:-}" =~ ^[0-9]+$ ]]; then
    echo "FAIL cannot parse size -B from $ev/size.txt"; return 1
  fi
  flash=$((text + data)); ram=$((data + bss))
  n_act=$(u10_members "$ev/zephyr.map" 'rcl_action|action_client|action_server|action_goal_handle')
  n_ts=$(u10_members "$ev/zephyr.map" 'rosidl_typesupport_c-')
  n_intro=$(u10_members "$ev/zephyr.map" 'introspection')
  {
    echo "RESULT $unit flash=$flash ram=$ram delta_flash=$((flash - U10_B0_FLASH)) delta_ram=$((ram - U10_B0_RAM))"
    echo "MEMBERS rcl_action=$n_act typesupport_c=$n_ts introspection=$n_intro"
  } | tee "$ev/result.txt"
  local ret=0
  if [ "$unit" = z ]; then
    local rf rr
    rf=$(sed -n 's/^flash=//p' "$U10_WT/$U10_REF" | head -n 1)
    rr=$(sed -n 's/^ram=//p' "$U10_WT/$U10_REF" | head -n 1)
    if [ "$flash" = "$rf" ] && [ "$ram" = "$rr" ]; then
      echo "CHECK baseline reproduces: OK" | tee -a "$ev/result.txt"
    else
      echo "CHECK baseline reproduces: DIFFERS" | tee -a "$ev/result.txt"; ret=1
    fi
  fi
  echo "evidence dir: $ev"
  return $ret
}
