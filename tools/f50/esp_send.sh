#!/system/bin/sh
# Use: sh /data/local/tmp/esp_send.sh PING
DEV=/dev/ttyACM0
if [ "$#" -eq 0 ]; then
  echo 'Usage: sh /data/local/tmp/esp_send.sh PING|STATUS|FAN_ON|FAN_OFF|AUTO'
  exit 1
fi
if [ ! -c "$DEV" ]; then
  echo "ERROR: $DEV not found"
  exit 1
fi
timeout 2 stty -F "$DEV" 115200 raw -echo -icanon min 0 time 10 || exit 1
timeout 3 cat "$DEV" &
reader=$!
sleep 0.2
timeout 2 /system/bin/sh -c 'printf "%s\n" "$1" > "$2"' sh "$*" "$DEV"
sent=$?
wait "$reader" 2>/dev/null
exit "$sent"
