#include <Arduino.h>

// ESP32-C3 SuperMini 推荐用 GPIO4 控 MOS
static const int FAN_MOS_GPIO = 8;

// 板载蓝灯 GPIO8
static const int LED_GPIO = 8;

// 大多数低边 N-MOS：高电平导通
static const bool FAN_ON_LEVEL = true;

// 板载 LED 逻辑可能是低电平点亮
// 如果发现 FAN_ON 时灯灭、FAN_OFF 时灯亮，把这个改成 false/true 反过来
static const bool LED_ON_LEVEL = true;

static bool fanState = false;

void setLed(bool on) {
  digitalWrite(LED_GPIO, on ? LED_ON_LEVEL : !LED_ON_LEVEL);
}

void setFan(bool on) {
  fanState = on;

  digitalWrite(FAN_MOS_GPIO, on ? FAN_ON_LEVEL : !FAN_ON_LEVEL);

  // 风扇开，板载灯亮；风扇关，板载灯灭
  setLed(on);

  Serial.print("OK FAN ");
  Serial.println(on ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(FAN_MOS_GPIO, OUTPUT);
  pinMode(LED_GPIO, OUTPUT);

  // 上电默认关闭风扇
  setFan(false);

  Serial.println("READY F50 FAN CTRL ESP32-C3");
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
  Serial.println("OK LOOP");
}