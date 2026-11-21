#!/system/bin/sh
CONTROL=/data/local/tmp/f50_fan_control.sh
PID_FILE=/data/local/tmp/f50_fan_control.lock
pid=$(cat "$PID_FILE" 2>/dev/null)
case "$pid" in ''|*[!0-9]*) pid= ;; esac
if [ -n "$pid" ] && [ -r "/proc/$pid/cmdline" ] &&
   tr '\000' '\n' < "/proc/$pid/cmdline" | grep -Fxq "$CONTROL"; then
  kill "$pid" || exit 1
  n=0
  while kill -0 "$pid" 2>/dev/null; do
    [ "$n" -ge 6 ] && { echo "ERROR: controller did not stop pid=$pid"; exit 1; }
    sleep 1
    n=$((n + 1))
  done
fi
# Keep AUTO intent for hysteresis; never force OFF as part of restarting.
sh /data/media/0/ufi_tools_boot.sh || exit 1
sleep 1
pid=$(cat "$PID_FILE" 2>/dev/null)
case "$pid" in ''|*[!0-9]*) echo 'ERROR: no controller PID'; exit 1 ;; esac
if kill -0 "$pid" 2>/dev/null &&
   tr '\000' '\n' < "/proc/$pid/cmdline" | grep -Fxq "$CONTROL"; then
  echo "fan control running pid=$pid"
else
  echo 'ERROR: controller failed to start'
  exit 1
fi
