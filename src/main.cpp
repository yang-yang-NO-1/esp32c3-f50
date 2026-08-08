#include <Arduino.h>

// ESP32-C3 SuperMini：风扇控制脚接 GPIO8
static const int FAN_MOS_GPIO = 8;

// 如果你的 MOS 是高电平导通，保持 true
// 如果你的风扇板是低电平导通，改成 false
static const bool FAN_ON_LEVEL = true;

static bool fanState = false;

void setFan(bool on) {
  fanState = on;

  digitalWrite(FAN_MOS_GPIO, on ? FAN_ON_LEVEL : !FAN_ON_LEVEL);

  Serial.print("OK FAN ");
  Serial.println(on ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(800);

  pinMode(FAN_MOS_GPIO, OUTPUT);

  // 上电后立即关闭风扇
  setFan(false);

  Serial.println("READY F50 FAN CTRL ESP32-C3 GPIO8");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "PING") {
      Serial.println("OK PONG");
    } else if (cmd == "FAN_ON") {
      setFan(true);
    } else if (cmd == "FAN_OFF") {
      setFan(false);
    } else if (cmd == "STATUS") {
      Serial.print("OK STATUS FAN=");
      Serial.println(fanState ? "ON" : "OFF");
    } else {
      Serial.print("ERR UNKNOWN ");
      Serial.println(cmd);
    }
  }

  delay(10);
}