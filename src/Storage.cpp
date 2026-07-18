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
}

String Storage::loadQuickRoll() { return prefs.getString("quick", "1D20"); }
void Storage::saveQuickRoll(const String& expression) { prefs.putString("quick", expression); }

String Storage::loadCombination(uint8_t slot) {
  if (slot >= 8) return "";
  String key = "combo" + String(slot);
  return prefs.getString(key.c_str(), "");
}

void Storage::saveCombination(uint8_t slot, const String& expression) {
  if (slot >= 8) return;
  String key = "combo" + String(slot);
  prefs.putString(key.c_str(), expression);
}

void Storage::clearCombination(uint8_t slot) {
  if (slot >= 8) return;
  String key = "combo" + String(slot);
  prefs.remove(key.c_str());
}

}  // namespace dicevessel
