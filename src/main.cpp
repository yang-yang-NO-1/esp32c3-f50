#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "control_state.h"
#include "web_page.h"
#include "portal_panel.h"

// GPIO8 also drives the SuperMini onboard LED: never blink it independently.
constexpr int FAN_GPIO = 8;
constexpr bool FAN_ON_LEVEL = HIGH;
constexpr int LINK_LED_GPIO = 4; // External LED + resistor to GND; -1 disables.
constexpr int WIFI_LED_GPIO = 5;
constexpr int BOOT_GPIO = 9;
constexpr uint32_t BOOT_HOLD_MS = 5000;
constexpr uint32_t BOOT_DEBOUNCE_MS = 40;
static_assert(LINK_LED_GPIO != FAN_GPIO && WIFI_LED_GPIO != FAN_GPIO,
              "Status LEDs must not share the fan output");
constexpr uint32_t LINK_FRESH_MS = 30000;
constexpr uint32_t TEST_TIMEOUT_MS = 15000;
constexpr uint32_t MANUAL_MS = 10 * 60 * 1000UL;
WiFiManager wifiManager;
// WiFiManager creates/recreates the only HTTP server on port 80.
WebServer &managementServer() { return *wifiManager.server; }
ControlState control;
String apName, csrfToken;
String lastCommand = "NONE";
uint32_t lastRx = 0, rxCount = 0, testStarted = 0, testStartCount = 0;
uint32_t manualStarted = 0, networkStarted = 0;
bool portalRequested = false, mdnsStarted = false, previousUsb = false;
const char *testResult = "idle";
bool bootRaw = false, bootPressed = false, bootHandled = false;
uint32_t bootChanged = 0, bootPressedAt = 0;
bool clearWifiRequested = false;
char rxBuffer[96];
size_t rxLength = 0;
bool rxOverflow = false;

void applyFan() { digitalWrite(FAN_GPIO, control.fanOn ? FAN_ON_LEVEL : !FAN_ON_LEVEL); }
bool usbPlugged() { return HWCDC::isPlugged(); }
bool linkFresh() {
  return usbPlugged() && rxCount > 0 && millis() - lastRx < LINK_FRESH_MS;
}
void resumeAuto() { control.resumeAuto(); applyFan(); }

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();
  if (command.isEmpty()) return;
  const bool known = command == "PING" || command == "STATUS" ||
    command == "FAN_ON" || command == "FAN_OFF" || command == "HEARTBEAT" || command == "AUTO";
  if (!known) { Serial.println("ERR UNKNOWN " + command); return; }
  lastRx = millis();
  ++rxCount;
  lastCommand = command;
  if (command == "PING") Serial.println("OK PONG");
  else if (command == "HEARTBEAT") Serial.println("OK HEARTBEAT");
  else if (command == "STATUS") {
    Serial.print("OK STATUS FAN=");
    Serial.println(control.fanOn ? "ON" : "OFF");
  } else if (command == "AUTO") {
    resumeAuto(); Serial.println("OK AUTO");
  } else {
    control.serialFan(command == "FAN_ON");
    applyFan();
    if (control.manual) Serial.println("ERR MANUAL_OVERRIDE");
    else Serial.println(control.fanOn ? "OK FAN ON" : "OK FAN OFF");
  }
}
void processSerial() {
  // Bounded parser: a partial or noisy line must not freeze web/LED processing.
  for (size_t budget = 0; budget < 128 && Serial.available(); ++budget) {
    const char c = Serial.read();
    if (c == '\n') {
      if (rxOverflow) Serial.println("ERR LINE_TOO_LONG");
      else { rxBuffer[rxLength] = '\0'; handleCommand(String(rxBuffer)); }
      rxLength = 0; rxOverflow = false;
    } else if (c != '\r') {
      if (rxLength < sizeof(rxBuffer) - 1) rxBuffer[rxLength++] = c;
      else rxOverflow = true;
    }
  }
}
String jsonString(const String &input) {
  String out = "\"";
  for (size_t i = 0; i < input.length(); ++i) {
    const uint8_t c = input[i];
    if (c == '"' || c == '\\') { out += '\\'; out += char(c); }
    else if (c < 32) {
      char escaped[7]; snprintf(escaped, sizeof(escaped), "\\u%04x", c); out += escaped;
    } else out += char(c);
  }
  return out + "\"";
}
void sendStatus() {
  const bool wifi = WiFi.status() == WL_CONNECTED;
  const uint32_t manualElapsed = millis() - manualStarted;
  String json;
  json.reserve(700);
  json = "{\"fan\":" + String(control.fanOn ? "true" : "false");
  json += ",\"manual\":" + String(control.manual ? "true" : "false");
  json += ",\"autoKnown\":" + String(control.autoKnown ? "true" : "false");
  json += ",\"autoFan\":" + String(control.autoOn ? "true" : "false");
  json += ",\"manualRemaining\":" + String(control.manual && manualElapsed < MANUAL_MS ? (MANUAL_MS - manualElapsed) / 1000 : 0);
  json += ",\"usb\":" + String(usbPlugged() ? "true" : "false");
  json += ",\"recent\":" + String(linkFresh() ? "true" : "false");
  json += ",\"rxCount\":" + String(rxCount);
  json += ",\"rxAge\":" + (rxCount ? String((millis() - lastRx) / 1000) : String("null"));
  json += ",\"lastCommand\":" + jsonString(lastCommand);
  json += ",\"test\":" + jsonString(testResult);
  json += ",\"wifi\":" + String(wifi ? "true" : "false");
  json += ",\"portal\":" + String(wifiManager.getConfigPortalActive() ? "true" : "false");
  json += ",\"ssid\":" + jsonString(wifi ? WiFi.SSID() : apName);
  json += ",\"ip\":" + jsonString(wifi ? WiFi.localIP().toString() : WiFi.softAPIP().toString());
  json += ",\"rssi\":" + String(wifi ? WiFi.RSSI() : 0);
  json += ",\"uptime\":" + String(millis() / 1000);
  json += ",\"token\":" + jsonString(csrfToken) + "}";
  managementServer().sendHeader("Cache-Control", "no-store");
  managementServer().send(200, "application/json; charset=utf-8", json);
}
bool acceptMutation() {
  if (!managementServer().hasHeader("X-Control-Token") || managementServer().header("X-Control-Token") != csrfToken) {
    managementServer().send(403, "application/json", "{\"error\":\"Refresh the management page\"}");
    return false;
  }
  return true;
}
void setupWeb() {
  const char *headers[] = {"X-Control-Token"}; managementServer().collectHeaders(headers, 1);
  managementServer().on("/fan", HTTP_GET, []() { managementServer().send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  managementServer().on("/api/status", HTTP_GET, sendStatus);
  managementServer().on("/api/fan", HTTP_POST, []() {
    if (!acceptMutation()) return;
    const String action = managementServer().arg("action");
    if (action == "auto") resumeAuto();
    else if (action == "on" || action == "off") {
      control.manualFan(action == "on"); manualStarted = millis(); applyFan();
    } else { managementServer().send(400, "application/json", "{\"error\":\"Invalid action\"}"); return; }
    sendStatus();
  });
  managementServer().on("/api/test", HTTP_POST, []() {
    if (!acceptMutation()) return;
    testStarted = millis(); testStartCount = rxCount;
    testResult = usbPlugged() ? "waiting" : "no_usb";
    sendStatus();
  });
  managementServer().on("/api/provision", HTTP_POST, []() {
    if (!acceptMutation()) return;
    portalRequested = true; sendStatus();
  });
  // Keep WiFiManager root, /wifi, DNS and captive-portal detection handlers.
}
void updateBootButton() {
  const uint32_t now = millis();
  const bool pressed = digitalRead(BOOT_GPIO) == LOW;
  if (pressed != bootRaw) { bootRaw = pressed; bootChanged = now; }
  if (now - bootChanged >= BOOT_DEBOUNCE_MS && bootPressed != bootRaw) {
    bootPressed = bootRaw;
    if (bootPressed) { bootPressedAt = now; bootHandled = false; }
  }
  if (bootPressed && bootRaw && !bootHandled && now - bootPressedAt >= BOOT_HOLD_MS) {
    bootHandled = true; // One erase per hold, rearm only after release + new press.
    clearWifiRequested = true;
  }
}
void updateNetwork() {
  if (clearWifiRequested) {
    clearWifiRequested = false;
    if (wifiManager.getConfigPortalActive()) wifiManager.stopConfigPortal();
    else if (wifiManager.getWebPortalActive()) wifiManager.stopWebPortal();
    WiFi.setAutoReconnect(false);
    wifiManager.resetSettings(); // Erase STA credentials, not fan state or AP password.
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    networkStarted = millis();
    portalRequested = true;
  }
  wifiManager.process();
  if (WiFi.status() == WL_CONNECTED) {
    networkStarted = millis();
    if (!mdnsStarted) {
      mdnsStarted = MDNS.begin("f50-fan");
      if (mdnsStarted) MDNS.addService("http", "tcp", 80);
    }
  } else if (millis() - networkStarted > 15000 && !wifiManager.getConfigPortalActive()) {
    portalRequested = true;
  }
  if (portalRequested) {
    portalRequested = false;
    if (!wifiManager.getConfigPortalActive()) {
      if (wifiManager.getWebPortalActive()) wifiManager.stopWebPortal();
      wifiManager.startConfigPortal(apName.c_str(), "12345678");
    }
  }
  // A successful WiFiManager save destroys its HTTP server; rebind our routes
  // via setWebServerCallback whenever a new STA/AP portal server is created.
  if (!wifiManager.getConfigPortalActive() && !wifiManager.getWebPortalActive()) {
    wifiManager.startWebPortal();
  }
}
void updateLeds() {
  const uint32_t now = millis(), phase = now % 2000;
  // F50: solid=recent command, double blink=USB only, slow blink=no USB.
  const bool linkLed = linkFresh() || (usbPlugged() ?
    (phase < 100 || (phase >= 250 && phase < 350)) : phase < 150);
  const bool wifiLed = bootPressed && !bootHandled ? now % 100 < 50 :
    wifiManager.getConfigPortalActive() ? now % 400 < 200 :
    (WiFi.status() == WL_CONNECTED || now % 2000 < 150);
  if (LINK_LED_GPIO >= 0) digitalWrite(LINK_LED_GPIO, linkLed ? HIGH : LOW);
  if (WIFI_LED_GPIO >= 0) digitalWrite(WIFI_LED_GPIO, wifiLed ? HIGH : LOW);
}
void setup() {
  pinMode(BOOT_GPIO, INPUT_PULLUP);
  digitalWrite(FAN_GPIO, !FAN_ON_LEVEL); pinMode(FAN_GPIO, OUTPUT); applyFan();
  if (LINK_LED_GPIO >= 0) pinMode(LINK_LED_GPIO, OUTPUT);
  if (WIFI_LED_GPIO >= 0) pinMode(WIFI_LED_GPIO, OUTPUT);
  Serial.begin(115200); Serial.setTxTimeoutMs(0); Serial.setDebugOutput(false);
  wifiManager.setDebugOutput(false);
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConnectTimeout(10); wifiManager.setConfigPortalTimeout(0);
  wifiManager.setHostname("f50-fan");
  wifiManager.setWebServerCallback(setupWeb);
  wifiManager.setCustomMenuHTML(PORTAL_PANEL);
  const char *menu[] = {"custom", "wifi", "info"};
  wifiManager.setMenu(menu, 3);
  WiFi.mode(WIFI_STA);
  apName = "F50-FAN-" + WiFi.macAddress().substring(12); apName.replace(":", "");
  char token[33];
  snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(),
    (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
  csrfToken = token;
  WiFi.setAutoReconnect(true); WiFi.begin(); networkStarted = millis();
  wifiManager.startWebPortal();
  Serial.println("READY F50 FAN CTRL ESP32-C3 GPIO8");
}
void loop() {
  processSerial();
  const bool usb = usbPlugged();
  if (previousUsb && !usb) {
    rxCount = 0; lastCommand = "NONE"; rxLength = 0; rxOverflow = false;
  }
  previousUsb = usb;
  if (control.manual && millis() - manualStarted >= MANUAL_MS) resumeAuto();
  if (strcmp(testResult, "waiting") == 0) {
    if (!usb) testResult = "no_usb";
    else if (rxCount != testStartCount) testResult = "received";
    else if (millis() - testStarted >= TEST_TIMEOUT_MS) testResult = "timeout";
  }
  updateBootButton(); updateNetwork(); updateLeds(); delay(1);
}
