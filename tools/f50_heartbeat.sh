#!/system/bin/sh
# Optional receive-link indicator for the ESP32 firmware. Run one instance only.
# It does not read the serial device, so esp_send.sh can still receive replies.
# It reports this sender's liveness, NOT the temperature control process health.
DEV=/dev/ttyACM0
while true; do
  if [ -c "$DEV" ]; then
    if stty -F "$DEV" 115200 raw -echo -icanon min 0 time 10 2>/dev/null; then
      printf 'HEARTBEAT\n' > "$DEV" 2>/dev/null
    fi
  fi
  sleep 5
done
