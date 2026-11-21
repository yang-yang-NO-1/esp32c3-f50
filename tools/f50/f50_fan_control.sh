#!/system/bin/sh
# F50 temperature controller v2. Keep the ESP32 manual override authoritative.
DEV=/dev/ttyACM0
SOC_TEMP=/sys/class/thermal/thermal_zone4/temp
ON_TEMP=50000
OFF_TEMP=40000
INTERVAL=5
LOG_FILE=/data/local/tmp/f50_fan_control.log
PID_FILE=/data/local/tmp/f50_fan_control.lock
STATE_FILE=/data/local/tmp/fan_state
LOG_LIMIT=262144

# flock releases automatically after exit; never unlink the flock file.
exec 9>/data/local/tmp/f50_fan_control.flock
# Android mksh marks extra FDs close-on-exec unless explicitly redirected.
flock -n 9 9>&9 || exit 0
printf '%s\n' "$$" > "$PID_FILE"
cleanup() {
  [ "$(cat "$PID_FILE" 2>/dev/null)" = "$$" ] && rm -f "$PID_FILE"
}
trap 'exit 0' TERM INT
trap cleanup EXIT

log() {
  size=$(stat -c %s "$LOG_FILE" 2>/dev/null)
  if [ "${size:-0}" -ge "$LOG_LIMIT" ]; then
    mv -f "$LOG_FILE" "$LOG_FILE.1"
  fi
  printf '%s %s\n' "$(date '+%F %T')" "$*" >> "$LOG_FILE"
}

state=$(cat "$STATE_FILE" 2>/dev/null)
case "$state" in ON|OFF) ;; *) state=UNKNOWN ;; esac
last_link=unknown
last_fault=unknown
last_state=$state
sample_count=0
log "v2 started pid=$$ ON=$ON_TEMP OFF=$OFF_TEMP interval=${INTERVAL}s"

while true; do
  temp=$(cat "$SOC_TEMP" 2>/dev/null)
  fault=0
  case "$temp" in ''|*[!0-9]*) fault=1 ;; esac
  if [ "$fault" -eq 0 ]; then
    # Reject implausible sensor values instead of treating a read failure as cold.
    if [ "$temp" -le 0 ] || [ "$temp" -ge 150000 ]; then fault=1; fi
  fi
  if [ "$fault" -eq 1 ]; then
    state=ON
  elif [ "$temp" -ge "$ON_TEMP" ]; then
    state=ON
  elif [ "$temp" -le "$OFF_TEMP" ]; then
    state=OFF
  elif [ "$state" = UNKNOWN ]; then
    state=OFF
  fi
  if [ "$fault" != "$last_fault" ]; then
    if [ "$fault" -eq 1 ]; then log 'ERROR temperature sensor invalid; requesting FAN_ON';
    else log "temperature sensor OK raw=$temp"; fi
    last_fault=$fault
  fi
  if [ "$state" != "$last_state" ]; then
    log "auto intent $last_state -> $state raw=${temp:-invalid}"
    last_state=$state
  fi

  link=missing
  if [ -c "$DEV" ]; then
    link=io_error
    # Bound both termios setup and writes: unplug must not stall the main loop.
    if timeout 2 stty -F "$DEV" 115200 raw -echo -icanon min 0 time 10 2>/dev/null 9>&-; then
      if timeout 2 /system/bin/sh -c 'printf "HEARTBEAT\nFAN_%s\n" "$1" > "$2"' sh "$state" "$DEV" 2>/dev/null 9>&-; then
        link=sent
        printf '%s\n' "$state" > "$STATE_FILE"
      fi
    fi
  fi
  if [ "$link" != "$last_link" ]; then
    log "serial=$link device=$DEV desired=$state (sent means write succeeded, not ACK verified)"
    last_link=$link
  fi
  # State file holds AUTO intent; web MANUAL can intentionally differ.
  if [ "$sample_count" -eq 0 ]; then
    log "sample raw=${temp:-invalid} fault=$fault desired=$state serial=$link"
  fi
  sample_count=$(((sample_count + 1) % 12))
  sleep "$INTERVAL" 9>&-
done
