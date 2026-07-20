#include "Storage.h"
#include <Preferences.h>

namespace dicevessel {

static Preferences prefs;

// Legacy namespace kept intentionally so upgrades retain user settings and combos.
void Storage::begin() { prefs.begin("dicebox", false); }

Settings Storage::loadSettings() {
  Settings s;
  s.volume = prefs.getUChar("volume", s.volume);
  s.brightness = prefs.getUChar("bright", s.brightness);
  s.shakeSensitivity = prefs.getUChar("shakeSens", s.shakeSensitivity);
  s.shakeEnabled = prefs.getBool("shake", s.shakeEnabled);
  s.clickEnabled = prefs.getBool("click", s.clickEnabled);
  s.chargeMode = prefs.getBool("charge", s.chargeMode);
  s.portuguese = prefs.getBool("ptbr", s.portuguese);
  s.accentColor = prefs.getUChar("accent", s.accentColor);
  s.volume = min<uint8_t>(s.volume, 10);
  s.brightness = constrain(s.brightness, 16, 250);
  s.shakeSensitivity = min<uint8_t>(s.shakeSensitivity, 10);
  if (s.accentColor > 7) s.accentColor = 0;
  return s;
}

void Storage::saveSettings(const Settings& s) {
  prefs.putUChar("volume", s.volume);
  prefs.putUChar("bright", s.brightness);
  prefs.putUChar("shakeSens", s.shakeSensitivity);
  prefs.putBool("shake", s.shakeEnabled);
  prefs.putBool("click", s.clickEnabled);
  prefs.putBool("charge", s.chargeMode);
  prefs.putBool("ptbr", s.portuguese);
  prefs.putUChar("accent", s.accentColor);
}

String Storage::loadQuickRoll() { return prefs.getString("quick", "1D20"); }
void Storage::saveQuickRoll(const String& expression) { prefs.putString("quick", expression); }

SavedCombination Storage::loadCombination(uint8_t slot) {
  SavedCombination saved;
  if (slot >= 8) return saved;
  String expressionKey = "combo" + String(slot);
  String nameKey = "name" + String(slot);
  String modeKey = "mode" + String(slot);
  saved.expression = prefs.getString(expressionKey.c_str(), "");
  saved.name = prefs.getString(nameKey.c_str(), "");
  saved.mode = static_cast<RollMode>(min<uint8_t>(prefs.getUChar(modeKey.c_str(), 0), 2));
  if (!saved.expression.isEmpty() && saved.name.isEmpty()) saved.name = "ROLL " + String(slot + 1);
  return saved;
}

void Storage::saveCombination(uint8_t slot, const SavedCombination& saved) {
  if (slot >= 8) return;
  String expressionKey = "combo" + String(slot);
  String nameKey = "name" + String(slot);
  String modeKey = "mode" + String(slot);
  prefs.putString(expressionKey.c_str(), saved.expression);
  prefs.putString(nameKey.c_str(), saved.name.substring(0, 18));
  prefs.putUChar(modeKey.c_str(), static_cast<uint8_t>(saved.mode));
}

void Storage::clearCombination(uint8_t slot) {
  if (slot >= 8) return;
  String key = "combo" + String(slot);
  prefs.remove(key.c_str());
  key = "name" + String(slot); prefs.remove(key.c_str());
  key = "mode" + String(slot); prefs.remove(key.c_str());
}

uint8_t Storage::loadHistory(HistoryEntry* entries, uint8_t capacity) {
  String packed = prefs.getString("history", "");
  uint8_t count = 0;
  int start = 0;
  while (start < packed.length() && count < capacity) {
    int end = packed.indexOf('\n', start);
    if (end < 0) end = packed.length();
    String line = packed.substring(start, end);
    int first = line.indexOf('|');
    int second = first < 0 ? -1 : line.indexOf('|', first + 1);
    if (first > 0 && second > first) {
      entries[count].mode = static_cast<RollMode>(constrain(line.substring(0, first).toInt(), 0, 2));
      entries[count].total = line.substring(first + 1, second).toInt();
      entries[count].expression = line.substring(second + 1);
      entries[count].timestampMs = 0;
      if (!entries[count].expression.isEmpty()) ++count;
    }
    start = end + 1;
  }
  return count;
}

void Storage::saveHistory(const HistoryEntry* entries, uint8_t count) {
  String packed;
  packed.reserve(count * 42);
  for (uint8_t i = 0; i < count; ++i) {
    packed += String(static_cast<uint8_t>(entries[i].mode)) + "|";
    packed += String(entries[i].total) + "|" + entries[i].expression + "\n";
  }
  prefs.putString("history", packed);
}

}  // namespace dicevessel
