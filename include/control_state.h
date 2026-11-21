#pragma once

// Cache F50 intent while a web override is active, so resuming AUTO does not
// depend on F50 sending another transition (its script only sends on changes).
struct ControlState {
  bool fanOn = false;
  bool manual = false;
  bool autoKnown = false;
  bool autoOn = false;
  void serialFan(bool on) {
    autoKnown = true;
    autoOn = on;
    if (!manual) fanOn = on;
  }
  void manualFan(bool on) { manual = true; fanOn = on; }
  void resumeAuto() {
    manual = false;
    // Retain output until the first host command if no host intent is known.
    if (autoKnown) fanOn = autoOn;
  }
};
