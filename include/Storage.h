#pragma once

#include <Arduino.h>

namespace dicevessel {

struct Settings {
  uint8_t volume = 6;
  uint8_t brightness = 160;
  uint8_t shakeSensitivity = 5;
  bool shakeEnabled = true;
  bool clickEnabled = true;
  bool chargeMode = true;
  bool portuguese = false;
};

class Storage {
 public:
  void begin();
  Settings loadSettings();
  void saveSettings(const Settings& value);
  String loadQuickRoll();
  void saveQuickRoll(const String& expression);
  String loadCombination(uint8_t slot);
  void saveCombination(uint8_t slot, const String& expression);
  void clearCombination(uint8_t slot);
};

}  // namespace dicevessel
