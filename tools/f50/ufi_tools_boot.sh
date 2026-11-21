#!/system/bin/sh
# Called by UFI-TOOLS as root; the controller owns the singleton lock and retries USB.
CONTROL=/data/local/tmp/f50_fan_control.sh
LOG=/data/local/tmp/f50_fan_autostart.log
if [ "$(stat -c %s "$LOG" 2>/dev/null)" -ge 262144 ] 2>/dev/null; then
  mv -f "$LOG" "$LOG.1"
fi
if [ ! -f "$CONTROL" ]; then
  printf '%s ERROR controller missing\n' "$(date '+%F %T')" >> "$LOG"
  exit 1
fi
nohup /system/bin/sh "$CONTROL" >>/data/local/tmp/f50_fan_control.out 2>&1 </dev/null &
printf '%s v2 controller launch requested pid=%s (USB retry in main loop)\n' "$(date '+%F %T')" "$!" >> "$LOG"
exit 0
