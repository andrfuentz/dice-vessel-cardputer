#pragma once

#include <Arduino.h>
#include "DiceModel.h"

namespace dicevessel {

struct Settings {
  uint8_t volume = 6;
  uint8_t brightness = 160;
  uint8_t shakeSensitivity = 5;
  bool shakeEnabled = true;
  bool clickEnabled = true;
  bool chargeMode = true;
  bool portuguese = false;
  uint8_t accentColor = 0;
};

struct SavedCombination {
  String name;
  String expression;
  RollMode mode = RollMode::Normal;

  bool empty() const { return expression.isEmpty(); }
};

class Storage {
 public:
  void begin();
  Settings loadSettings();
  void saveSettings(const Settings& value);
  String loadQuickRoll();
  void saveQuickRoll(const String& expression);
  SavedCombination loadCombination(uint8_t slot);
  void saveCombination(uint8_t slot, const SavedCombination& combination);
  void clearCombination(uint8_t slot);
  uint8_t loadHistory(HistoryEntry* entries, uint8_t capacity);
  void saveHistory(const HistoryEntry* entries, uint8_t count);
};

}  // namespace dicevessel
