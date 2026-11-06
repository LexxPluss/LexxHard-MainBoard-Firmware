#!/usr/bin/env bash
# Measure the real rate (Hz) and bandwidth of the MainBoard v6 topics delivered by the CURRENT rosserial.
# Run it ON THE ROBOT HOST (the PC that runs LexxAuto with HW_VER=v6), with the ROS environment and roscore up.
# Usage: ./rosserial_baseline_measure.sh <condition-label> [seconds]
#   condition-label: free text without spaces, e.g. idle_stopped, driving, actuator_moving, charging
#   seconds: window per topic (default 60)
# Run it once per condition (idle, driving, actuator moving, charging) and keep the output directories.
# Read-only for the robot: it only subscribes (rostopic hz / bw). It does not publish anything.
set -u

LABEL="${1:-}"
SECS="${2:-60}"
[ -n "$LABEL" ] || { echo "usage: $0 <condition-label> [seconds]"; exit 2; }

OUT="${HOME}/rosserial_baseline_$(date +%Y%m%d_%H%M%S)_${LABEL}"
mkdir -p "$OUT"

# shellcheck disable=SC1091
source /opt/ros/noetic/setup.bash 2>/dev/null || true
command -v rostopic >/dev/null || { echo "rostopic not found (source the ROS environment)"; exit 2; }
rostopic list >/dev/null 2>&1 || { echo "roscore not reachable"; exit 2; }

# Names on the host after the rosserial_server remaps (driver_launch/launch/main.launch:105-137, outlier_filter not involved).
# If a name is absent on this robot, the script records that and goes on.
TOPICS=(
  /driver/internal/sensor_set/imu
  /driver/internal/sensor_set/pgv
  /driver/internal/sensor_set/ultrasonic
  /driver/internal/sensor_set/downward
  /driver/internal/sensor_set/towing_unit
  /driver/internal/sensor_set/battery
  /driver/internal/sensor_set/bumper
  /driver/internal/sensor_set/fan
  /global/system/emergency_switch
  /global/system/temperature
  /global/system/charge_heartbeat_delay
  /driver/internal/body_control/encoder_count
  /driver/internal/body_control/linear_actuator_current
  /driver/internal/body_control/shelf_connection
  /driver/internal/body_control/charge_status
  /driver/internal/body_control/power_state
  /driver/internal/body_control/charge_connector_voltage
  /global/emergency_stop_at_connected_robot
  /global/lexxhard/dfu_response
  /diagnostics
)

{
  echo "date: $(date -Is)"
  echo "condition: $LABEL"
  echo "seconds per topic: $SECS (all topics measured in parallel)"
  echo "host: $(hostname)"
  echo "HW_VER: ${HW_VER:-unset}"
  echo "rostopic list -v (MainBoard related):"
  rostopic list -v 2>/dev/null | grep -E "sensor_set|body_control|lexxhard|emergency|system/|diagnostics" || true
} > "$OUT/info.txt"

pids=()
for t in "${TOPICS[@]}"; do
  n="$(echo "$t" | tr '/' '_' | sed 's/^_//')"
  if ! rostopic list 2>/dev/null | grep -Fxq "$t"; then
    echo "ABSENT $t" > "$OUT/${n}.absent"
    continue
  fi
  # rostopic hz and bw each time out after SECS seconds; SIGINT makes them print the final statistics.
  ( timeout -s INT "$SECS" rostopic hz  "$t" > "$OUT/${n}.hz.txt" 2>&1 ) &
  pids+=($!)
  ( timeout -s INT "$SECS" rostopic bw  "$t" > "$OUT/${n}.bw.txt" 2>&1 ) &
  pids+=($!)
done

# Whole-link figure: bytes per second of the rosserial node's serial line is not visible from ROS; record the sum from the .bw files.
for p in "${pids[@]}"; do wait "$p" 2>/dev/null; done

{
  echo "== last statistics per topic ($LABEL, ${SECS}s) =="
  for f in "$OUT"/*.hz.txt; do
    [ -e "$f" ] || continue
    echo "--- $(basename "$f" .hz.txt)"
    grep -E "average rate|min:|max:" "$f" | tail -2
  done
  for f in "$OUT"/*.bw.txt; do
    [ -e "$f" ] || continue
    echo "--- $(basename "$f" .bw.txt) bw"
    tail -2 "$f"
  done
  ls "$OUT"/*.absent 2>/dev/null | sed 's|.*/||' | sed 's/^/ABSENT: /'
} > "$OUT/summary.txt"

echo "OUT=$OUT"
echo "SUMMARY condition=$LABEL seconds=$SECS topics=${#TOPICS[@]} dir=$OUT"
