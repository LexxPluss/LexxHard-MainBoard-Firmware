#!/usr/bin/env bash
# Shared pieces of the U12 build scripts (sourced by u12a1/a2/b/c/d/e_build.sh, not run directly).
# Every unit builds the spike firmware with FW_UNIT=h and the U10e fragment (LTO, wait loop, direct type support),
# then runs the unit's checks (TESTPLAN section 8.3). Adapted from u10_common.sh.
# u12_run <unit>: without U12_WORKER it runs the worker under loop_run.sh (stdout = loop_run.sh lines only);
# with U12_WORKER=1 it is the worker: all output goes to the log through loop_run.sh.
U12_HERE="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
U12_LOOP_RUN="$U12_HERE/loop_run.sh"
U12_WT="$(git -C "$U12_HERE" rev-parse --show-toplevel)"
U12_BRANCH="feat/zephyr-4.1-microros-spike"
U12_IMG=zephyrprojectrtos/zephyr-build:v0.27.4
U12_SCRIPTS="$U12_HERE"
U12_DESC_OPT="-DROSIDL_GENERATOR_C_DISABLE_TYPE_DESCRIPTION_CODEGEN=ON"
U12_OVERLAY=spike_fw/variants/microros_nano_nodesc.conf
U12_FRAGMENT=spike_fw/variants/u10e_combined.conf
U12_SESSIONS_FILE=spike_fw/variants/u12_sessions.txt   # content: the number for RMW_UXRCE_MAX_SESSIONS (repository-driven)
U12_SLOT_BYTES=262144

u12_ensure_workspace() {
  # Fresh clone: run setup_workspace.sh first when anything the build assumes is missing; no-op otherwise.
  local m=$U12_WT/micro_ros_zephyr_module/modules/libmicroros
  if [ -e "$U12_WT/.west" ] && [ -e "$U12_WT/zephyr" ] && [ -f "$U12_WT/bootloader/mcuboot/root-rsa-2048.pem" ] \
     && [ -d "$m/micro_ros_src/src" ] && docker image inspect "$U12_IMG" >/dev/null 2>&1; then
    return 0
  fi
  echo "workspace incomplete, running setup_workspace.sh"
  local out rc
  out=$("$U12_HERE/setup_workspace.sh")
  rc=$?
  echo "$out"
  return $rc
}

u12_run() {
  local unit=$1 self
  self=$(readlink -f "${BASH_SOURCE[1]}")
  if [ "${U12_WORKER:-}" = 1 ]; then
    u12_worker "$unit" "$self"
    return $?
  fi
  local stamp log rc
  u12_ensure_workspace || return $?
  stamp=$(date +%Y%m%d_%H%M%S)
  log=${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u12_${unit}_${stamp}.log
  mkdir -p ${SPIKE_LOG_DIR:-$HOME/log}
  : > "$log"
  "$U12_LOOP_RUN" --kind build --log "$log" --repo "$U12_WT" --branch "$U12_BRANCH" -- \
    env U12_WORKER=1 U12_STAMP="$stamp" U12_LOG="$log" bash "$self"
  rc=$?
  return $rc
}

u12_provenance() {  # u12_provenance <dir> <self> <module-dir> <orig-mk>
  local d=$1 self=$2 m=$3 orig=$4
  {
    echo "date: $(date -Is)"
    echo "script: $self md5 $(md5sum "$self" | cut -d' ' -f1)"
    echo "common: ${BASH_SOURCE[0]} md5 $(md5sum "${BASH_SOURCE[0]}" | cut -d' ' -f1)"
    echo "entity list: $U12_HERE/u12_entities.txt md5 $(md5sum "$U12_HERE/u12_entities.txt" | cut -d' ' -f1)"
    echo "image: $U12_IMG id $(docker image inspect "$U12_IMG" --format '{{.Id}}')"
    echo "worktree HEAD: $(git -C "$U12_WT" rev-parse HEAD)  branch: $(git -C "$U12_WT" branch --show-current)"
    echo "worktree status (tracked changes):"; git -C "$U12_WT" status --short | grep -v '^??' || echo "  none"
    echo "west.yml md5: $(md5sum "$U12_WT/lexxpluss_apps/west.yml" | cut -d' ' -f1)"
    echo "module (micro_ros_zephyr_module) HEAD: $(git -C "$U12_WT/micro_ros_zephyr_module" rev-parse HEAD 2>&1)"
    echo "libmicroros.mk md5 (original): $(md5sum "$orig" | cut -d' ' -f1)"
    echo "sessions file: $( [ -f "$U12_WT/$U12_SESSIONS_FILE" ] && echo "$U12_SESSIONS_FILE = $(cat "$U12_WT/$U12_SESSIONS_FILE")" || echo none)"
    echo "app files md5 (spike_fw):"; (cd "$U12_WT" && find spike_fw -type f | sort | xargs md5sum)
    echo "extra package md5:"; (cd "$U12_WT/spike_microros_types/extra_packages" && find . -type f | sort | xargs md5sum)
    echo "ROS 2 source repositories in micro_ros_src/src (HEAD each):"
    find "$m/micro_ros_src/src" -maxdepth 2 -name .git | sort | while read -r g; do
      r=$(dirname "$g"); echo "  ${r#$m/micro_ros_src/src/} $(git -C "$r" rev-parse HEAD 2>&1) $(git -C "$r" remote get-url origin 2>&1)"
    done
  } > "$d/provenance.txt" 2>&1
}

u12_checks() {  # u12_checks <unit> <evidence-dir> <build-dir> <module-dir>; prints CHECK lines; returns non-zero on any failure
  local unit=$1 ev=$2 b=$3 m=$4 rc=0
  local map="$ev/zephyr.map"
  case "$unit" in
    a1)  # E3: unit h links without the sample, with the two per-port session entry points
      local sample uart6 uart2
      # symbols of the ELF (robust under LTO, where the map no longer lists object names)
      sample=$(readelf -sW "$ev/zephyr.elf" 2>/dev/null | awk '$8=="lexxhard_microros_run"' | wc -l)
      uart6=$(readelf -sW "$ev/zephyr.elf" 2>/dev/null | awk '$8=="lexxhard_microros_session_uart6"' | wc -l)
      uart2=$(readelf -sW "$ev/zephyr.elf" 2>/dev/null | awk '$8=="lexxhard_microros_session_uart2"' | wc -l)
      echo "CHECK E3 sample_symbols=$sample session_uart6=$uart6 session_uart2=$uart2"
      [ "$sample" -eq 0 ] && [ "$uart6" -gt 0 ] && [ "$uart2" -gt 0 ] || { echo "FAIL E3"; rc=1; } ;;
    a2)  # E4: sessions in the built config.h
      local want have cfg
      want=$(cat "$U12_WT/$U12_SESSIONS_FILE" 2>/dev/null || echo 1)
      cfg=$(find "$m" -path '*rmw_microxrcedds_c/config.h' 2>/dev/null | head -n 1)
      have=$( [ -n "$cfg" ] && sed -n 's/^#define RMW_UXRCE_MAX_SESSIONS[[:space:]]*\([0-9]*\).*/\1/p' "$cfg" | head -n 1 )
      echo "CHECK E4 sessions=${have:-unknown} expected=2 sessions_file=${want}"
      [ "${have:-}" = "2" ] || { echo "FAIL E4 sessions=${have:-unknown} expected 2"; rc=1; } ;;
    b)  "$U12_HERE/u12_check_entities.sh" "$U12_WT" pub || rc=1 ;;
    c)  "$U12_HERE/u12_check_entities.sh" "$U12_WT" pub,sub || rc=1 ;;
    d)  "$U12_HERE/u12_check_entities.sh" "$U12_WT" all || rc=1 ;;
    e|f)  # E6: real slot size in the built dts, signed image, margin
      local reg slot0 slot1 signed
      reg=$(awk '/partition@40000/{f=1} f&&/reg = </{print; exit}' "$b/zephyr/zephyr.dts")
      slot0=$(echo "$reg" | sed -n 's/.*reg = <[[:space:]]*0x40000[[:space:]]*\(0x[0-9a-fA-F]*\).*/\1/p')
      reg=$(awk '/partition@80000/{f=1} f&&/reg = </{print; exit}' "$b/zephyr/zephyr.dts")
      slot1=$(echo "$reg" | sed -n 's/.*reg = <[[:space:]]*0x80000[[:space:]]*\(0x[0-9a-fA-F]*\).*/\1/p')
      signed=$(stat -c %s "$b/zephyr/zephyr.signed.bin" 2>/dev/null || echo 0)
      echo "CHECK E6 slot0_size=${slot0:-unknown} slot1_size=${slot1:-unknown} expected=0x40000 signed_image_bytes=$signed margin_bytes_before_trailer=$((U12_SLOT_BYTES - signed))"
      [ "${slot0:-}" = "0x40000" ] && [ "${slot1:-}" = "0x40000" ] && [ "$signed" -gt 0 ] && [ "$signed" -le "$U12_SLOT_BYTES" ] || { echo "FAIL E6"; rc=1; }
      if [ "$unit" = f ]; then
        # E8: pools of the built rmw config.h (guards against a cached libmicroros.a built with other values)
        local cfg v_nodes v_sess v_srv v_sub v_pub
        cfg=$(find "$m" -path '*rmw_microxrcedds_c/config.h' 2>/dev/null | head -n 1)
        v_nodes=$(sed -n 's/^#define RMW_UXRCE_MAX_NODES[[:space:]]*\([0-9]*\).*/\1/p' "$cfg" | head -n 1)
        v_sess=$(sed -n 's/^#define RMW_UXRCE_MAX_SESSIONS[[:space:]]*\([0-9]*\).*/\1/p' "$cfg" | head -n 1)
        v_srv=$(sed -n 's/^#define RMW_UXRCE_MAX_SERVICES[[:space:]]*\([0-9]*\).*/\1/p' "$cfg" | head -n 1)
        v_sub=$(sed -n 's/^#define RMW_UXRCE_MAX_SUBSCRIPTIONS[[:space:]]*\([0-9]*\).*/\1/p' "$cfg" | head -n 1)
        v_pub=$(sed -n 's/^#define RMW_UXRCE_MAX_PUBLISHERS[[:space:]]*\([0-9]*\).*/\1/p' "$cfg" | head -n 1)
        echo "CHECK E8 nodes=${v_nodes:-unknown} expected=2 sessions=${v_sess:-unknown} services=${v_srv:-unknown} subscriptions=${v_sub:-unknown} publishers=${v_pub:-unknown}"
        [ "${v_nodes:-}" = 2 ] && [ "${v_sess:-}" = 2 ] && [ "${v_srv:-}" = 3 ] && [ "${v_sub:-}" = 12 ] && [ "${v_pub:-0}" -ge 20 ] || { echo "FAIL E8"; rc=1; }
        # E9: update image and confirmed image
        local conf
        conf=$(stat -c %s "$b/zephyr/zephyr.signed.confirmed.bin" 2>/dev/null || echo 0)
        echo "CHECK E9 update_image_bytes=$signed confirmed_image_bytes=$conf expected_confirmed=$U12_SLOT_BYTES update_md5=$(md5sum "$b/zephyr/zephyr.signed.bin" | cut -d' ' -f1) confirmed_md5=$(md5sum "$b/zephyr/zephyr.signed.confirmed.bin" | cut -d' ' -f1)"
        [ "$conf" -eq "$U12_SLOT_BYTES" ] || { echo "FAIL E9"; rc=1; }
        "$U12_HERE/u12_check_entities.sh" "$U12_WT" all || rc=1
      fi ;;
  esac
  return $rc
}

u12_worker() {  # u12_worker <unit> <self>
  local unit=$1 self=$2 bd=build-fw-u12$1 rc out confs cmake_extra=""
  local m=$U12_WT/micro_ros_zephyr_module/modules/libmicroros
  local mk=$m/libmicroros.mk
  local ev=${SPIKE_LOG_DIR:-$HOME/log}/zephyr4_u12_${unit}_${U12_STAMP}
  out=${U12_LOG%.log}.build.txt
  echo "u12 worker: unit=$unit stamp=$U12_STAMP $(date -Is)"
  case "$unit" in a1|a2|b|c|d|e|f) ;; *) echo "BUILD_RC=1"; echo "FAIL unknown unit: $unit"; return 1 ;; esac
  docker image inspect "$U12_IMG" >/dev/null 2>&1 || { echo "BUILD_RC=1"; echo "FAIL image absent: $U12_IMG"; return 1; }
  [ -d "$U12_WT/spike_fw" ] || { echo "BUILD_RC=1"; echo "FAIL spike_fw absent"; return 1; }
  [ -f "$U12_WT/$U12_OVERLAY" ] && [ -f "$U12_WT/$U12_FRAGMENT" ] || { echo "BUILD_RC=1"; echo "FAIL overlay or fragment absent"; return 1; }
  [ -d "$m/micro_ros_src/src" ] || { echo "BUILD_RC=1"; echo "FAIL module work tree absent"; return 1; }
  [ -f "$U12_HERE/u12_entities.txt" ] || { echo "BUILD_RC=1"; echo "FAIL entity list absent"; return 1; }
  if grep -q "DISABLE_TYPE_DESCRIPTION" "$mk"; then echo "BUILD_RC=1"; echo "FAIL libmicroros.mk is already patched (restore it first)"; return 1; fi
  confs="/workdir/$U12_OVERLAY;/workdir/$U12_FRAGMENT"
  case "$unit" in e|f) cmake_extra="-DSPIKE_REAL_SLOT=ON" ;; esac

  mkdir -p "$ev"
  cp "$mk" "$ev/libmicroros.mk.orig"
  trap "cp '$ev/libmicroros.mk.orig' '$mk'" EXIT
  echo "== module work tree: extra package, std_srvs marker, generated files, mk patches"
  rm -rf "$m/micro_ros_src/src/lexxauto_msgs"
  cp -r "$U12_WT/spike_microros_types/extra_packages/lexxauto_msgs" "$m/micro_ros_src/src/lexxauto_msgs"
  rm -f "$m/micro_ros_src/src/common_interfaces/std_srvs/COLCON_IGNORE"
  rm -rf "$m/micro_ros_src/build" "$m/micro_ros_src/install" "$m/micro_ros_src/log" "$m/micro_ros_src/src/log" \
         "$m/include" "$m/libmicroros.a" "$m/configured_colcon.meta" "$m/zephyr_toolchain.cmake"
  # patch 1 (as in U10): no type description code generation
  sed -i 's|^\([[:space:]]*\)-DTHIRDPARTY=ON \\$|&\n\1'"$U12_DESC_OPT"' \\|' "$mk"
  grep -q -- "$U12_DESC_OPT" "$mk" || { echo "BUILD_RC=1"; echo "FAIL mk patch (description) not applied"; return 1; }
  # patch 2 (repository-driven): sessions, only when the repository holds spike_fw/variants/u12_sessions.txt
  if [ -f "$U12_WT/$U12_SESSIONS_FILE" ]; then
    local n; n=$(tr -d '[:space:]' < "$U12_WT/$U12_SESSIONS_FILE")
    case "$n" in ''|*[!0-9]*) echo "BUILD_RC=1"; echo "FAIL sessions file is not a number: $n"; return 1 ;; esac
    python3 -I - "$mk" "$n" <<'PY' || { echo "BUILD_RC=1"; echo "FAIL mk patch (sessions) not applied"; return 1; }
import sys
p, n = sys.argv[1], sys.argv[2]
t = open(p).read()
old = 'update_meta "rmw_microxrcedds" "RMW_UXRCE_TRANSPORT=custom";'
assert t.count(old) == 1
t = t.replace(old, 'update_meta "rmw_microxrcedds" "RMW_UXRCE_MAX_SESSIONS=%s"; \\\n\t' % n + old)
open(p, "w").write(t)
PY
  fi
  diff -u "$ev/libmicroros.mk.orig" "$mk" > "$ev/libmicroros.mk.patch"; cat "$ev/libmicroros.mk.patch"
  u12_provenance "$ev" "$self" "$m" "$ev/libmicroros.mk.orig"
  cp "$U12_WT/lexxpluss_apps/west.yml" "$ev/west.yml"

  echo "== build $bd (FW_UNIT=h, EXTRA_CONF_FILE=$confs $cmake_extra)"
  docker run --rm -v "$U12_WT":/workdir -w /workdir -e ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
    -e BD="$bd" -e CONFS="$confs" -e CMAKE_EXTRA="$cmake_extra" "$U12_IMG" bash -c '
    export ZEPHYR_SDK_INSTALL_DIR=$(ls -d /opt/toolchains/zephyr-sdk-* | head -1); echo "SDK: $ZEPHYR_SDK_INSTALL_DIR"
    west config zephyr.base-prefer configfile
    pip3 install --user catkin_pkg lark-parser "empy==3.3.4" colcon-common-extensions 2>&1 | tail -1
    export PATH=$HOME/.local/bin:$PATH
    pip3 freeze 2>/dev/null | grep -i -E "catkin|lark|empy|colcon" | sed "s/^/PIP: /"
    rm -rf "$BD"
    west build -p always -b lexxpluss_mb02 spike_fw -d "$BD" -- -DFW_UNIT=h "-DEXTRA_CONF_FILE=$CONFS" $CMAKE_EXTRA 2>&1
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
  ls -l "$U12_WT/$bd/zephyr/zephyr.elf"

  echo "== evidence $ev"
  local b=$U12_WT/$bd
  cp "$out" "$ev/build.log"
  grep -E "^PIP:|BUILD_RC=" "$ev/build.log" >> "$ev/provenance.txt" 2>&1
  cp "$b/zephyr/zephyr.elf" "$b/zephyr/zephyr.map" "$b/zephyr/.config" "$b/zephyr/zephyr.dts" "$ev/"
  cp "$b"/zephyr/zephyr.signed*.bin "$ev/" 2>/dev/null
  cp "$m/libmicroros.a" "$ev/libmicroros.a"
  { echo "zephyr.elf md5 $(md5sum "$ev/zephyr.elf" | cut -d' ' -f1) size $(stat -c %s "$ev/zephyr.elf")"
    echo "libmicroros.a md5 $(md5sum "$ev/libmicroros.a" | cut -d' ' -f1) size $(stat -c %s "$ev/libmicroros.a")"
    echo "overlay md5 $(md5sum "$U12_WT/$U12_OVERLAY" | cut -d' ' -f1)"
    echo "fragment md5 $(md5sum "$U12_WT/$U12_FRAGMENT" | cut -d' ' -f1)"; } | tee -a "$ev/provenance.txt"
  docker run --rm -v "$ev":/in:ro "$U12_IMG" bash -c '
    S=$(ls -d /opt/toolchains/zephyr-sdk-* | head -1)/arm-zephyr-eabi/bin/arm-zephyr-eabi-size; $S -B /in/zephyr.elf; $S -A /in/zephyr.elf' 2>/dev/null \
    | grep -v -E "Openbox|Xlib|VNC|PORT=" > "$ev/size.txt"
  echo "size -B:"; sed -n '1,2p' "$ev/size.txt"
  grep -E "^[[:space:]]*(FLASH|RAM):" "$out" | head -n 2 | sed 's/^/LINKER /'

  local text data bss flash ram
  read -r text data bss _ < <(sed -n '2p' "$ev/size.txt")
  if ! [[ "${text:-}" =~ ^[0-9]+$ && "${data:-}" =~ ^[0-9]+$ && "${bss:-}" =~ ^[0-9]+$ ]]; then
    echo "FAIL cannot parse size -B from $ev/size.txt"; return 1
  fi
  flash=$((text + data)); ram=$((data + bss))
  echo "RESULT $unit flash=$flash ram=$ram" | tee "$ev/result.txt"
  local ret=0
  u12_checks "$unit" "$ev" "$b" "$m" | tee -a "$ev/result.txt"
  [ "${PIPESTATUS[0]}" -eq 0 ] || ret=1
  if [ "$unit" = f ] && [ "$ret" -eq 0 ]; then
    mkdir -p "$U12_WT/out"
    cp "$b/zephyr/zephyr.signed.bin" "$b/zephyr/zephyr.signed.confirmed.bin" "$U12_WT/out/" || ret=1
    echo "IMAGES out/zephyr.signed.bin md5 $(md5sum "$U12_WT/out/zephyr.signed.bin" | cut -d' ' -f1)" \
      "out/zephyr.signed.confirmed.bin md5 $(md5sum "$U12_WT/out/zephyr.signed.confirmed.bin" | cut -d' ' -f1)"
  fi
  echo "evidence dir: $ev"
  return $ret
}
