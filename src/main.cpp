#include <M5Cardputer.h>
#include <M5Unified.h>
#include <esp_system.h>
#include <math.h>

#include "DiceModel.h"
#include "ExpressionParser.h"
#include "PhysicsEngine.h"
#include "Storage.h"

#ifndef DICE_VESSEL_VERSION
#define DICE_VESSEL_VERSION "dev"
#endif

using namespace dicevessel;

namespace {

constexpr uint16_t C_BG = 0x0000;
constexpr uint16_t C_PANEL = 0x0000;
constexpr uint16_t C_RED = 0xF9C7;
constexpr uint16_t C_SHADOW = 0x0000;

struct AccentPalette {
  uint16_t main;
  uint16_t bright;
  uint16_t dim;
  uint16_t dark;
};

constexpr AccentPalette ACCENT_PALETTES[] = {
    {0xFD20, 0xFFE0, 0x7A80, 0x3180},  // Amber
    {0x07E0, 0x9FE7, 0x03E0, 0x0180},  // Terminal green
    {0x07FF, 0xBFFF, 0x03EF, 0x0186},  // Cyan
    {0xA81F, 0xE39F, 0x600F, 0x2806},  // Violet
    {0xC618, 0xFFFF, 0x7BEF, 0x2945},  // Monochrome
    {0xFAF4, 0xFE1B, 0x89AB, 0x38A4},  // Pink
    {0xF208, 0xFCD1, 0x88E3, 0x3861},  // Red
    {0xD81F, 0xFBDF, 0x7010, 0x3007},  // Magenta
};
constexpr uint8_t ACCENT_COLOR_COUNT = sizeof(ACCENT_PALETTES) / sizeof(ACCENT_PALETTES[0]);

uint16_t C_WOOD = ACCENT_PALETTES[0].main;
uint16_t C_WOOD_DIM = ACCENT_PALETTES[0].dim;
uint16_t C_WOOD_DARK = ACCENT_PALETTES[0].dark;
uint16_t C_CREAM = ACCENT_PALETTES[0].bright;
uint16_t C_GREEN = ACCENT_PALETTES[0].bright;

enum class Screen { Roller, Wizard, Combos, ComboName, Options, History, Help, About, Moment, Charge };
enum class Phase { Idle, Shaking, Decaying, Result };
enum class ResultEvent { None, Critical, Failure };
enum class ConfirmAction { None, DeleteCombination, ClearHistory, ResetSettings };

M5Canvas canvas(&M5Cardputer.Display);
PhysicsEngine physics;
Storage storage;
Settings settings;
Screen screen = Screen::Roller;
Phase phase = Phase::Idle;
String expression = "1D20";
RollSpec spec;
RollResult lastResult;
RollMode rollMode = RollMode::Normal;
HistoryEntry history[10];
SavedCombination combinations[8];
uint8_t historyCount = 0;
uint8_t optionIndex = 0;
uint8_t optionTab = 0;
uint8_t optionRow = 0;
uint8_t historyIndex = 0;
uint8_t comboIndex = 0;
uint8_t helpPage = 0;
int guidedField = 0;
bool guidedMode = true;
bool typingFresh = true;
bool imuAvailable = false;
float shakeEnergy = 0;
uint32_t lastMotionAt = 0;
uint32_t lastFrameAt = 0;
uint32_t lastDrawAt = 0;
uint32_t lastImpactSoundAt = 0;
uint32_t phaseStartedAt = 0;
uint32_t lastInputAt = 0;
uint32_t chargeDismissedAt = 0;
bool clickSequence = false;
uint32_t clickReleaseAt = 0;
uint32_t nextClickKickAt = 0;
String toast;
uint32_t toastUntil = 0;
ResultEvent resultEvent = ResultEvent::None;
ConfirmAction confirmAction = ConfirmAction::None;
uint8_t confirmSlot = 0;
uint32_t confirmUntil = 0;
bool settingsDirty = false;
bool historyDirty = false;
uint32_t settingsSaveAt = 0;
uint32_t historySaveAt = 0;
String aboutCode;
uint32_t aboutCodeAt = 0;
uint32_t momentStartedAt = 0;

DiceTerm wizardTerms[4];
uint8_t wizardTermCount = 0;
uint8_t wizardStep = 0;
uint8_t wizardReviewRow = 0;
uint16_t wizardSides = 20;
uint8_t wizardQty = 1;
int16_t wizardModifier = 0;
RollMode wizardMode = RollMode::Normal;
String comboNameBuffer;
String comboSaveExpression;
RollMode comboSaveMode = RollMode::Normal;
uint8_t comboSaveSlot = 0;

struct EditField { uint8_t start, end; enum Kind { Count, Sides, Modifier } kind; };
EditField fields[24];
uint8_t fieldCount = 0;

const char* ui(const char* pt, const char* en) {
  return settings.portuguese ? pt : en;
}

void applyAccentPalette() {
  settings.accentColor %= ACCENT_COLOR_COUNT;
  const AccentPalette& palette = ACCENT_PALETTES[settings.accentColor];
  C_WOOD = palette.main;
  C_CREAM = palette.bright;
  C_GREEN = palette.bright;
  C_WOOD_DIM = palette.dim;
  C_WOOD_DARK = palette.dark;
}

const char* accentName() {
  static const char* namesPt[] = {"AMBAR", "VERDE", "CIANO", "VIOLETA", "BRANCO", "ROSA", "VERMELHO", "MAGENTA"};
  static const char* namesEn[] = {"AMBER", "GREEN", "CYAN", "VIOLET", "WHITE", "PINK", "RED", "MAGENTA"};
  return settings.portuguese ? namesPt[settings.accentColor] : namesEn[settings.accentColor];
}

const char* rollModeName(RollMode mode, bool shortName = false) {
  if (mode == RollMode::Advantage) return shortName ? "ADV" : ui("VANTAGEM", "ADVANTAGE");
  if (mode == RollMode::Disadvantage) return shortName ? "DIS" : ui("DESVANTAGEM", "DISADVANTAGE");
  return shortName ? "NOR" : ui("NORMAL", "NORMAL");
}

bool supportsRollMode(const RollSpec& roll) {
  return roll.termCount == 1 && roll.terms[0].sign > 0 && roll.terms[0].count == 1 && roll.terms[0].sides == 20;
}

String uiError(const String& error) {
  if (settings.portuguese || error.isEmpty()) return error;
  if (error == "EXPRESSAO VAZIA") return "EMPTY EXPRESSION";
  if (error == "TERMO INCOMPLETO") return "INCOMPLETE TERM";
  if (error == "NUMERO ESPERADO") return "NUMBER EXPECTED";
  if (error == "NUMERO MUITO GRANDE") return "NUMBER TOO LARGE";
  if (error == "MAXIMO 64 DADOS") return "MAXIMUM 64 DICE";
  if (error == "FACES ESPERADAS") return "SIDES EXPECTED";
  if (error == "DADO NAO SUPORTADO") return "UNSUPPORTED DIE";
  if (error == "MUITOS TERMOS") return "TOO MANY TERMS";
  if (error == "MODIFICADOR INVALIDO") return "INVALID MODIFIER";
  if (error == "CARACTERE INVALIDO") return "INVALID CHARACTER";
  if (error == "ADICIONE UM DADO") return "ADD A DIE";
  return error;
}

void showToast(const String& text, uint32_t duration = 1300) {
  toast = text; toastUntil = millis() + duration;
}

void queueSettingsSave() {
  settingsDirty = true;
  settingsSaveAt = millis() + 700;
}

void queueHistorySave() {
  historyDirty = true;
  historySaveAt = millis() + 400;
}

bool confirmDestructive(ConfirmAction action, uint8_t slot, const String& prompt) {
  const uint32_t now = millis();
  if (confirmAction == action && confirmSlot == slot && static_cast<int32_t>(confirmUntil - now) > 0) {
    confirmAction = ConfirmAction::None;
    confirmUntil = 0;
    return true;
  }
  confirmAction = action;
  confirmSlot = slot;
  confirmUntil = now + 2400;
  showToast(prompt, 2200);
  return false;
}

void beginCombinationName(uint8_t slot, const String& savedExpression, RollMode savedMode, const String& currentName = "") {
  comboSaveSlot = constrain(slot, 0, 7);
  comboSaveExpression = savedExpression;
  comboSaveMode = savedMode;
  comboNameBuffer = currentName.substring(0, 18);
  screen = Screen::ComboName;
}

void renderSplashFrame(int progress) {
  canvas.fillScreen(C_BG);
  canvas.setTextDatum(middle_center);
  canvas.setTextColor(C_WOOD, C_BG);
  canvas.setTextSize(2);
  canvas.drawString("DICE\\VESSEL", 120, 50);
  canvas.setTextSize(1);
  canvas.setTextColor(C_WOOD_DIM, C_BG);
  canvas.drawString("KEEP ROLLING.", 120, 78);
  canvas.drawFastHLine(62, 98, 116, C_WOOD_DARK);
  canvas.drawFastHLine(62, 98, constrain(progress, 0, 116), C_WOOD);
  canvas.setTextDatum(top_left);
}

void drawSplash() {
  const uint32_t startedAt = millis();
  while (millis() - startedAt < 1450) {
    M5Cardputer.update();
    const auto keys = M5Cardputer.Keyboard.keysState();
    if (keys.enter) break;
    renderSplashFrame(static_cast<int>((millis() - startedAt) * 116 / 1450));
    canvas.pushSprite(0, 0);
    delay(16);
  }
}

uint32_t uniformRandom(uint32_t limit) {
  if (limit < 2) return 0;
  const uint32_t threshold = static_cast<uint32_t>(-limit) % limit;
  uint32_t r;
  do { r = esp_random(); } while (r < threshold);
  return r % limit;
}

RollResult generateResult(const RollSpec& roll, const String& source) {
  RollResult out;
  out.expression = ExpressionParser::normalize(source);
  out.mode = roll.mode;
  out.total = roll.modifier;
  if (roll.mode != RollMode::Normal && supportsRollMode(roll)) {
    const uint16_t first = 1 + uniformRandom(20);
    const uint16_t second = 1 + uniformRandom(20);
    const bool keepFirst = roll.mode == RollMode::Advantage ? first >= second : first <= second;
    out.dice[0].sides = 20; out.dice[0].value = first; out.dice[0].sign = 1; out.dice[0].kept = keepFirst;
    out.dice[1].sides = 20; out.dice[1].value = second; out.dice[1].sign = 1; out.dice[1].kept = !keepFirst;
    out.count = 2;
    out.total += keepFirst ? first : second;
    return out;
  }
  for (uint8_t t = 0; t < roll.termCount; ++t) {
    const DiceTerm& term = roll.terms[t];
    for (uint8_t n = 0; n < term.count && out.count < MAX_RESULTS; ++n) {
      DieResult& die = out.dice[out.count++];
      die.sides = term.sides; die.sign = term.sign;
      die.value = 1 + uniformRandom(term.sides);
      out.total += die.sign * die.value;
    }
  }
  return out;
}

void addHistory(const RollResult& result) {
  for (int i = min<int>(historyCount, 9); i > 0; --i) history[i] = history[i - 1];
  history[0].expression = result.expression;
  history[0].total = result.total;
  history[0].mode = result.mode;
  history[0].timestampMs = millis();
  if (historyCount < 10) ++historyCount;
  queueHistorySave();
}

void rebuildFields() {
  fieldCount = 0;
  const String s = ExpressionParser::normalize(expression);
  uint8_t i = 0;
  while (i < s.length() && fieldCount < 22) {
    uint8_t signedStart = i;
    if (s[i] == '+' || s[i] == '-') ++i;
    uint8_t start = i;
    while (i < s.length() && isdigit(static_cast<unsigned char>(s[i]))) ++i;
    if (start == i) break;
    if (i < s.length() && s[i] == 'D') {
      fields[fieldCount++] = {start, i, EditField::Count};
      uint8_t faceStart = ++i;
      while (i < s.length() && isdigit(static_cast<unsigned char>(s[i]))) ++i;
      fields[fieldCount++] = {faceStart, i, EditField::Sides};
    } else {
      fields[fieldCount++] = {signedStart, i, EditField::Modifier};
    }
  }
  bool hasModifier = false;
  for (uint8_t n = 0; n < fieldCount; ++n) if (fields[n].kind == EditField::Modifier) hasModifier = true;
  if (!hasModifier && fieldCount < 24) fields[fieldCount++] = {static_cast<uint8_t>(s.length()), static_cast<uint8_t>(s.length()), EditField::Modifier};
  if (fieldCount == 0) guidedField = 0;
  else guidedField = constrain(guidedField, 0, fieldCount - 1);
}

void refreshSpec(bool respawn = true) {
  expression = ExpressionParser::normalize(expression);
  spec = ExpressionParser::parse(expression);
  if (rollMode != RollMode::Normal && (!spec.valid || !supportsRollMode(spec))) rollMode = RollMode::Normal;
  spec.mode = rollMode;
  rebuildFields();
  if (respawn && spec.valid && phase == Phase::Idle) physics.configure(spec, 240, 23, 118);
}

void replaceField(int delta) {
  if (!fieldCount) return;
  EditField f = fields[guidedField];
  int value = expression.substring(f.start, f.end).toInt();
  if (f.kind == EditField::Sides) {
    const int sides[] = {2, 4, 6, 8, 10, 12, 20, 100};
    int index = 0;
    while (index < 8 && sides[index] != value) ++index;
    index = (index + delta + 8) % 8;
    value = sides[index];
  } else if (f.kind == EditField::Count) {
    value = constrain(value + delta, 1, 64);
  } else {
    value = constrain(value + delta, -999, 999);
    String replacement = value > 0 ? "+" + String(value) : (value < 0 ? String(value) : "");
    expression = expression.substring(0, f.start) + replacement + expression.substring(f.end);
    refreshSpec();
    return;
  }
  expression = expression.substring(0, f.start) + String(value) + expression.substring(f.end);
  refreshSpec();
}

int8_t impactBuffers[3][448];
uint8_t impactBufferIndex = 0;
int8_t momentSamples[1600];

void playWoodKnock(float strength, bool light = false) {
  if (!settings.volume) return;
  impactBufferIndex = (impactBufferIndex + 1) % 3;
  int8_t* samples = impactBuffers[impactBufferIndex];
  const int length = light ? 220 : constrain(250 + static_cast<int>(strength), 280, 448);
  const float baseHz = light ? 420.0f : constrain(310.0f - strength * 0.55f, 150.0f, 290.0f);
  uint32_t noise = esp_random();
  float filtered = 0;
  for (int i = 0; i < length; ++i) {
    noise = noise * 1664525u + 1013904223u;
    float raw = static_cast<int8_t>(noise >> 24) / 128.0f;
    filtered = filtered * 0.62f + raw * 0.38f;
    float env = expf(-i / (light ? 55.0f : 88.0f));
    float body = sinf(TWO_PI * baseHz * i / 16000.0f) * 0.62f;
    float attack = i < 18 ? (1.0f - i / 18.0f) * filtered * 0.85f : filtered * 0.22f;
    samples[i] = static_cast<int8_t>(constrain((body + attack) * env * 105.0f, -120.0f, 120.0f));
  }
  M5Cardputer.Speaker.setVolume(min(255, settings.volume * 29));
  M5Cardputer.Speaker.playRaw(samples, length, 16000, false, 1, -1, false);
}

void playCoinSound() {
  if (!settings.volume) return;
  impactBufferIndex = (impactBufferIndex + 1) % 3;
  int8_t* samples = impactBuffers[impactBufferIndex];
  const int length = 448;
  for (int i = 0; i < length; ++i) {
    float env = expf(-i / 210.0f);
    float ping = sinf(TWO_PI * 2350.0f * i / 16000.0f) * 0.62f;
    ping += sinf(TWO_PI * 3870.0f * i / 16000.0f) * 0.28f;
    ping += sinf(TWO_PI * 5350.0f * i / 16000.0f) * 0.10f;
    samples[i] = static_cast<int8_t>(constrain(ping * env * 118.0f, -125.0f, 125.0f));
  }
  M5Cardputer.Speaker.setVolume(min(255, settings.volume * 30));
  M5Cardputer.Speaker.playRaw(samples, length, 16000, false, 1, -1, true);
}

void playResultEvent(bool critical) {
  if (!settings.volume) return;
  impactBufferIndex = (impactBufferIndex + 1) % 3;
  int8_t* samples = impactBuffers[impactBufferIndex];
  const int length = 448;
  for (int i = 0; i < length; ++i) {
    float t = i / 16000.0f;
    float env = expf(-i / 260.0f);
    float hz = critical ? (520.0f + i * 2.2f) : (330.0f - i * 0.42f);
    float wave = sinf(TWO_PI * hz * t) + 0.35f * sinf(TWO_PI * hz * 1.5f * t);
    samples[i] = static_cast<int8_t>(constrain(wave * env * 78.0f, -122.0f, 122.0f));
  }
  M5Cardputer.Speaker.setVolume(min(255, settings.volume * 30));
  M5Cardputer.Speaker.playRaw(samples, length, 16000, false, critical ? 2 : 1, -1, true);
}

void playMomentSound() {
  if (!settings.volume) return;
  uint32_t noise = esp_random();
  float filtered = 0;
  for (int i = 0; i < 1600; ++i) {
    const float t = i / 16000.0f;
    const float knockEnv = expf(-i / 125.0f);
    const float chimeEnv = i < 180 ? 0.0f : expf(-(i - 180) / 620.0f);
    noise = noise * 1664525u + 1013904223u;
    filtered = filtered * 0.68f + (static_cast<int8_t>(noise >> 24) / 128.0f) * 0.32f;
    float wave = filtered * knockEnv * 0.72f;
    wave += sinf(TWO_PI * 1040.0f * t) * chimeEnv * 0.48f;
    wave += sinf(TWO_PI * 1560.0f * t) * chimeEnv * 0.27f;
    wave += sinf(TWO_PI * 2080.0f * t) * chimeEnv * 0.13f;
    momentSamples[i] = static_cast<int8_t>(constrain(wave * 112.0f, -124.0f, 124.0f));
  }
  M5Cardputer.Speaker.setVolume(min(255, settings.volume * 30));
  M5Cardputer.Speaker.playRaw(momentSamples, 1600, 16000, false, 1, -1, true);
}

void beginMoment() {
  aboutCode = "";
  momentStartedAt = millis();
  screen = Screen::Moment;
  playMomentSound();
}

void woodImpact(float strength) {
  if (!settings.volume || millis() - lastImpactSoundAt < 42 || strength < 24) return;
  lastImpactSoundAt = millis();
  playWoodKnock(strength);
}

void revealTick() {
  playWoodKnock(18, true);
}

void releaseRoll() {
  clickSequence = false;
  if (!spec.valid) { showToast(uiError(spec.error)); phase = Phase::Idle; return; }
  lastResult = generateResult(spec, expression);
  physics.beginDecay(lastResult, millis());
  phase = Phase::Decaying;
  phaseStartedAt = millis();
}

void clickRoll() {
  if (!settings.clickEnabled || (phase != Phase::Idle && phase != Phase::Result)) return;
  refreshSpec(false);
  if (!spec.valid) { showToast(uiError(spec.error)); return; }
  physics.configure(spec, 240, 23, 118);
  phase = Phase::Shaking;
  phaseStartedAt = millis();
  clickSequence = true;
  const bool coin = spec.diceCount == 1 && spec.terms[0].sides == 2;
  clickReleaseAt = millis() + (coin ? 260 : 820);
  nextClickKickAt = millis() + (coin ? 150 : 72);
  physics.excite(coin ? 1.0f : 3.15f);
  typingFresh = true;
}

void finishRoll() {
  if (phase == Phase::Result) return;
  phase = Phase::Result;
  addHistory(lastResult);
  resultEvent = ResultEvent::None;
  bool hasMax = false, hasOne = false;
  if (supportsRollMode(spec)) {
    for (uint8_t i = 0; i < lastResult.count; ++i) {
      if (lastResult.dice[i].kept && lastResult.dice[i].sides == 20) {
        hasMax |= lastResult.dice[i].value == 20;
        hasOne |= lastResult.dice[i].value == 1;
      }
    }
  }
  if (hasMax) resultEvent = ResultEvent::Critical;
  else if (hasOne) resultEvent = ResultEvent::Failure;
  if (lastResult.count == 1 && lastResult.dice[0].sides == 2) playCoinSound();
  else if (resultEvent != ResultEvent::None) playResultEvent(resultEvent == ResultEvent::Critical);
  else revealTick();
}

void drawHeader(const String& title) {
  canvas.fillRect(0, 0, 240, 22, C_BG);
  canvas.setTextColor(C_WOOD, C_PANEL);
  canvas.setTextSize(1);
  canvas.setCursor(5, 5);
  canvas.print(title);
  int level = M5Cardputer.Power.getBatteryLevel();
  String battery = level >= 0 ? String(level) + "%" : "--%";
  canvas.setCursor(238 - battery.length() * 6, 5);
  canvas.print(battery);
  canvas.drawFastHLine(0, 20, 240, C_WOOD_DARK);
}

void drawFooter(const String& text, uint16_t color = 0) {
  if (!color) color = C_WOOD_DIM;
  canvas.fillRect(0, 118, 240, 17, C_BG);
  canvas.drawFastHLine(0, 117, 240, C_WOOD_DARK);
  canvas.setTextSize(1);
  canvas.setTextColor(color, C_BG);
  canvas.setCursor(max(4, 120 - static_cast<int>(text.length()) * 3), 123);
  canvas.print(text);
}

void drawDieShape(const Body& b, bool reveal) {
  const int x = static_cast<int>(b.x), y = static_cast<int>(b.y);
  const int r = static_cast<int>(b.radius);
  const uint16_t edge = reveal && !b.kept ? C_WOOD_DIM : C_CREAM;
  const float speed = sqrtf(b.vx * b.vx + b.vy * b.vy);
  canvas.fillEllipse(x + 2, y + r + 1, max(2, r - 2), 2, C_WOOD_DARK);

  if (!reveal && speed > 75.0f) {
    const int trailX = x - constrain(static_cast<int>(b.vx * 0.018f), -7, 7);
    const int trailY = y - constrain(static_cast<int>(b.vy * 0.018f), -5, 5);
    canvas.drawEllipse(trailX, trailY, max(2, r - 3), max(2, r - 4), C_WOOD_DARK);
  }

  if (b.sides == 2) {
    int width = reveal ? r * 2 : max(2, static_cast<int>(fabsf(cosf(b.angle)) * r * 2));
    canvas.fillEllipse(x, y, width / 2, r, C_WOOD_DARK);
    canvas.drawEllipse(x, y, width / 2, r, edge);
    if (reveal || width > 8) canvas.drawEllipse(x, y, max(1, width / 2 - 3), max(2, r - 3), C_WOOD_DIM);
  } else {
    const int vertices = b.sides == 4 ? 3 : (b.sides == 6 || b.sides == 8 ? 4 : (b.sides == 10 ? 5 : 6));
    float offset = b.angle - HALF_PI;
    if (b.sides == 6) offset += PI * 0.25f;
    for (int i = 0; i < vertices; ++i) {
      float a1 = offset + i * TWO_PI / vertices;
      float a2 = offset + (i + 1) * TWO_PI / vertices;
      const int x1 = x + cosf(a1) * r, y1 = y + sinf(a1) * r;
      const int x2 = x + cosf(a2) * r, y2 = y + sinf(a2) * r;
      canvas.fillTriangle(x, y, x1, y1, x2, y2, (i & 1) ? C_WOOD_DARK : C_BG);
      canvas.drawLine(x + cosf(a1) * r, y + sinf(a1) * r,
                      x + cosf(a2) * r, y + sinf(a2) * r, edge);
      if (!reveal || b.sides >= 12) canvas.drawLine(x, y, x1, y1, C_WOOD_DIM);
    }
    if (b.sides == 8) {
      canvas.drawFastHLine(x - r / 2, y, r, C_WOOD_DIM);
    } else if (b.sides == 20) {
      const int inner = max(2, r / 2);
      canvas.drawTriangle(x, y - inner, x - inner, y + inner / 2, x + inner, y + inner / 2, C_WOOD_DIM);
    } else if (b.percentilePart == 1 && !reveal) {
      canvas.drawEllipse(x, y, max(2, r / 3), max(2, r / 3), edge);
    }
  }

  if (reveal && (b.result || b.percentilePart)) {
    String value;
    if (b.percentilePart == 1) value = b.result ? String(b.result) : "00";
    else value = String(b.result);
    const uint8_t textSize = r >= 16 && value.length() <= 2 ? 2 : 1;
    canvas.setTextSize(textSize);
    const int textWidth = canvas.textWidth(value);
    const int textHeight = textSize == 2 ? 16 : 8;
    const int badgeWidth = min(r * 2 - 4, textWidth + 6);
    canvas.fillRoundRect(x - badgeWidth / 2, y - textHeight / 2, badgeWidth, textHeight, 2, C_BG);
    canvas.drawRoundRect(x - badgeWidth / 2, y - textHeight / 2, badgeWidth, textHeight, 2, C_WOOD_DIM);
    canvas.setTextColor(b.kept ? C_CREAM : C_WOOD_DIM, C_BG);
    canvas.setCursor(x - textWidth / 2, y - textHeight / 2 + (textSize == 2 ? 1 : 0));
    canvas.print(value);
    canvas.setTextSize(1);
    if (b.kept && lastResult.mode != RollMode::Normal) canvas.drawFastHLine(x - r / 2, y + r - 2, r, C_CREAM);
  }
}

void drawResultEvent() {
  if (resultEvent == ResultEvent::None || phase != Phase::Result) return;
  String eventText = resultEvent == ResultEvent::Critical ? ui("CRITICO!", "CRITICAL!") : ui("FALHA!", "FAILURE!");
  canvas.fillRoundRect(76, 25, 88, 18, 3, C_WOOD);
  canvas.drawRoundRect(76, 25, 88, 18, 3, C_CREAM);
  canvas.setTextColor(C_BG, C_WOOD);
  canvas.setCursor(120 - eventText.length() * 3, 31);
  canvas.print(eventText);
}

void drawRoller() {
  canvas.fillScreen(C_BG);
  String header = "DICE\\VESSEL  " + expression;
  if (rollMode != RollMode::Normal) header += " [" + String(rollModeName(rollMode, true)) + "]";
  drawHeader(header);
  canvas.drawFastHLine(0, 22, 240, C_WOOD_DARK);

  for (uint8_t i = 0; i < physics.count(); ++i) {
    const Body& body = physics.body(i);
    drawDieShape(body, body.settled || phase == Phase::Result);
  }
  drawResultEvent();
  if (physics.hiddenCount()) {
    canvas.setTextColor(C_CREAM, C_PANEL);
    canvas.fillRoundRect(194, 27, 41, 15, 3, C_PANEL);
    canvas.setCursor(200, 31);
    canvas.print("+" + String(physics.hiddenCount()));
  }

  if (phase == Phase::Idle) {
    if (!spec.valid) drawFooter(uiError(spec.error), C_RED);
    else if (imuAvailable && settings.shakeEnabled) drawFooter(ui("SHAKE/ENT R NOVA C SALVAS M MENU", "SHAKE/ENT R NEW C SAVED M MENU"), C_CREAM);
    else drawFooter(ui("ENTER ROLA R NOVA C SALVAS M MENU", "ENTER ROLL R NEW C SAVED M MENU"), C_CREAM);
  } else if (phase == Phase::Shaking) {
    canvas.setTextColor(C_CREAM, C_BG); canvas.setCursor(86, 124); canvas.print(ui("SACUDINDO...", "SHAKING..."));
  } else if (phase == Phase::Decaying) {
    canvas.setTextColor(C_WOOD, C_BG); canvas.setCursor(84, 124); canvas.print(ui("ROLANDO...", "ROLLING..."));
  } else {
    canvas.fillRect(0, 114, 240, 21, C_BG);
    canvas.drawFastHLine(0, 113, 240, C_WOOD_DARK);
    canvas.setTextColor(C_GREEN, C_PANEL); canvas.setTextSize(2);
    String total = (lastResult.mode == RollMode::Normal ? String("") : String(rollModeName(lastResult.mode, true)) + "  ") + "TOTAL " + String(lastResult.total);
    canvas.setCursor(120 - total.length() * 6, 116); canvas.print(total);
    canvas.setTextSize(1);
  }

  if (guidedMode && fieldCount && (phase == Phase::Idle || phase == Phase::Result)) {
    const EditField& f = fields[guidedField];
    int prefixWidth = canvas.textWidth(expression.substring(0, f.start));
    int fieldWidth = canvas.textWidth(expression.substring(f.start, f.end));
    int baseX = 5 + canvas.textWidth("DICE\\VESSEL  ");
    if (f.start == f.end) canvas.drawFastHLine(baseX + prefixWidth, 17, 12, C_GREEN);
    else canvas.drawFastHLine(baseX + prefixWidth, 17, fieldWidth, C_GREEN);
  }

}

void drawOptions() {
  canvas.fillScreen(C_BG); drawHeader(ui("OPCOES", "OPTIONS"));
  const char* tabsPt[] = {"AUDIO", "MOV", "VISUAL", "SISTEMA"};
  const char* tabsEn[] = {"AUDIO", "MOTION", "VISUAL", "SYSTEM"};
  int tabX[] = {3, 57, 108, 165};
  for (uint8_t i = 0; i < 4; ++i) {
    if (i == optionTab) canvas.fillRect(tabX[i], 23, i == 3 ? 72 : 50, 14, C_WOOD_DARK);
    canvas.setTextColor(C_WOOD, i == optionTab ? C_WOOD_DARK : C_BG);
    canvas.setCursor(tabX[i] + 4, 27); canvas.print(settings.portuguese ? tabsPt[i] : tabsEn[i]);
  }
  canvas.drawFastHLine(0, 39, 240, C_WOOD_DARK);
  auto row = [&](uint8_t index, int y, const String& label, const String& value, int barValue) {
    if (index == optionRow) canvas.fillRect(4, y - 2, 232, 14, C_WOOD_DARK);
    uint16_t bg = index == optionRow ? C_WOOD_DARK : C_BG;
    canvas.setTextColor(C_WOOD, bg); canvas.setCursor(9, y); canvas.print(label);
    canvas.setCursor(226 - value.length() * 6, y); canvas.print(value);
    if (barValue >= 0) {
      int bx = 104, by = y + 10, bw = 120;
      canvas.drawRect(bx, by, bw, 5, C_WOOD_DIM);
      canvas.fillRect(bx + 1, by + 1, (bw - 2) * barValue / 10, 3, C_WOOD);
    }
  };
  if (optionTab == 0) row(0, 48, "VOLUME", String(settings.volume) + "/10", settings.volume);
  else if (optionTab == 1) {
    row(0, 48, "SHAKE", settings.shakeEnabled ? "ON" : "OFF", -1);
    row(1, 72, ui("SENSIBILIDADE", "SENSITIVITY"), String(settings.shakeSensitivity) + "/10", settings.shakeSensitivity);
    row(2, 96, "CLICK TO ROLL", settings.clickEnabled ? "ON" : "OFF", -1);
  } else if (optionTab == 2) {
    int brightness10 = settings.brightness <= 16 ? 0 : constrain((settings.brightness + 12) / 25, 1, 10);
    row(0, 44, ui("BRILHO", "BRIGHTNESS"), String(brightness10) + "/10", brightness10);
    row(1, 68, ui("COR PRINCIPAL", "ACCENT COLOR"), accentName(), -1);
    row(2, 92, ui("MODO CARGA", "CHARGE MODE"), settings.chargeMode ? "ON" : "OFF", -1);
    canvas.fillRoundRect(197, 81, 25, 5, 2, C_WOOD);
  } else {
    row(0, 43, ui("IDIOMA", "LANGUAGE"), settings.portuguese ? "PORTUGUES" : "ENGLISH", -1);
    row(1, 58, ui("INSTRUCOES", "INSTRUCTIONS"), "ENTER", -1);
    row(2, 73, ui("SOBRE", "ABOUT"), "ENTER", -1);
    row(3, 88, ui("BATERIA", "BATTERY"), "ENTER", -1);
    row(4, 103, ui("RESTAURAR PADROES", "RESTORE DEFAULTS"), "ENTER", -1);
  }
  drawFooter(ui("SETAS NAVEGAM  [ ] ALTERA  M VOLTA", "ARROWS NAVIGATE  [ ] CHANGE  M BACK"));
}

void drawHistory() {
  canvas.fillScreen(C_BG); drawHeader(ui("HISTORICO DA SESSAO", "SESSION HISTORY"));
  if (!historyCount) {
    canvas.setTextColor(C_WOOD, C_BG); canvas.setCursor(58, 66); canvas.print(ui("NENHUMA ROLAGEM", "NO ROLLS YET"));
  }
  for (uint8_t i = 0; i < min<uint8_t>(historyCount, 6); ++i) {
    int y = 28 + i * 15;
    if (i == historyIndex) canvas.fillRoundRect(4, y - 2, 232, 14, 3, C_WOOD_DARK);
    canvas.setTextColor(C_CREAM, i == historyIndex ? C_WOOD_DARK : C_BG);
    String mode = history[i].mode == RollMode::Normal ? "" : String("[") + rollModeName(history[i].mode, true) + "] ";
    String summary = mode + history[i].expression;
    if (summary.length() > 25) summary = summary.substring(0, 24) + "~";
    canvas.setCursor(8, y); canvas.print(summary);
    String total = "= " + String(history[i].total);
    canvas.setCursor(232 - total.length() * 6, y); canvas.print(total);
  }
  drawFooter(ui("ENTER REPETE  DEL LIMPA  M VOLTA", "ENTER REPEAT  DEL CLEAR  M BACK"));
}

String wizardExpression() {
  String out;
  for (uint8_t i = 0; i <= wizardTermCount && i < 4; ++i) {
    if (i) out += "+";
    uint8_t count = i == wizardTermCount ? wizardQty : wizardTerms[i].count;
    uint16_t sides = i == wizardTermCount ? wizardSides : wizardTerms[i].sides;
    out += String(count) + "D" + String(sides);
  }
  if (wizardModifier > 0) out += "+" + String(wizardModifier);
  else if (wizardModifier < 0) out += String(wizardModifier);
  return out;
}

void drawWizard() {
  canvas.fillScreen(C_BG);
  const char* titlesPt[] = {"1/5 TIPO DE DADO", "2/5 QUANTIDADE", "3/5 BONUS / PENALIDADE", "4/5 MODO D20", "5/5 CONFIRMAR"};
  const char* titlesEn[] = {"1/5 DIE TYPE", "2/5 QUANTITY", "3/5 BONUS / PENALTY", "4/5 D20 MODE", "5/5 CONFIRM"};
  drawHeader(settings.portuguese ? titlesPt[wizardStep] : titlesEn[wizardStep]);
  canvas.setTextColor(C_WOOD, C_BG);
  if (wizardStep == 0) {
    canvas.setTextSize(3); String value = "D" + String(wizardSides);
    canvas.setCursor(120 - value.length() * 9, 48); canvas.print(value); canvas.setTextSize(1);
    canvas.setCursor(62, 91); canvas.print(ui("[ / ] MUDA O DADO", "[ / ] CHANGE DIE"));
  } else if (wizardStep == 1) {
    canvas.setTextSize(4); String value = String(wizardQty);
    canvas.setCursor(120 - value.length() * 12, 42); canvas.print(value); canvas.setTextSize(1);
    canvas.setCursor(48, 95); canvas.print(ui("QUANTOS DADOS DESTE TIPO?", "HOW MANY DICE?"));
  } else if (wizardStep == 2) {
    canvas.setTextSize(4); String value = wizardModifier > 0 ? "+" + String(wizardModifier) : String(wizardModifier);
    canvas.setCursor(120 - value.length() * 12, 42); canvas.print(value); canvas.setTextSize(1);
    canvas.setCursor(37, 95); canvas.print(ui("APLICADO AO TOTAL FINAL", "APPLIED TO FINAL TOTAL"));
  } else if (wizardStep == 3) {
    const bool eligible = wizardTermCount == 0 && wizardQty == 1 && wizardSides == 20;
    if (!eligible) wizardMode = RollMode::Normal;
    String value = rollModeName(wizardMode);
    canvas.setTextSize(value.length() > 10 ? 2 : 3);
    canvas.setCursor(120 - value.length() * (value.length() > 10 ? 6 : 9), 47); canvas.print(value); canvas.setTextSize(1);
    canvas.setCursor(eligible ? 42 : 28, 94);
    canvas.print(eligible ? ui("[ / ] MUDA O MODO", "[ / ] CHANGE MODE") : ui("DISPONIVEL APENAS PARA 1D20", "AVAILABLE ONLY FOR 1D20"));
  } else {
    String value = wizardExpression();
    if (wizardMode != RollMode::Normal) value += " [" + String(rollModeName(wizardMode, true)) + "]";
    const uint8_t expressionSize = value.length() <= 18 ? 2 : 1;
    canvas.setTextSize(expressionSize);
    canvas.setCursor(120 - value.length() * (expressionSize == 2 ? 6 : 3), expressionSize == 2 ? 27 : 31);
    canvas.print(value); canvas.setTextSize(1);
    const char* actionsPt[] = {"ROLAR AGORA", "+ ADICIONAR DADO", "SALVAR COMBINACAO", "CANCELAR"};
    const char* actionsEn[] = {"ROLL NOW", "+ ADD DIE", "SAVE COMBINATION", "CANCEL"};
    for (uint8_t i = 0; i < 4; ++i) {
      int y = 54 + i * 16;
      if (i == wizardReviewRow) canvas.fillRect(18, y - 2, 204, 14, C_WOOD_DARK);
      canvas.setTextColor(C_WOOD, i == wizardReviewRow ? C_WOOD_DARK : C_BG);
      canvas.setCursor(28, y); canvas.print(settings.portuguese ? actionsPt[i] : actionsEn[i]);
    }
  }
  drawFooter(wizardStep == 4 ? ui("SETAS MOVE  ENTER OK  M VOLTA", "ARROWS MOVE  ENTER OK  M BACK") : ui("[ ] ALTERA  ENTER PROX  M VOLTA", "[ ] CHANGE  ENTER NEXT  M BACK"));
}

void drawCombos() {
  canvas.fillScreen(C_BG); drawHeader(ui("COMBINACOES SALVAS", "SAVED COMBINATIONS"));
  uint8_t start = comboIndex >= 6 ? 2 : 0;
  for (uint8_t i = 0; i < 6; ++i) {
    uint8_t slot = start + i;
    int y = 27 + i * 15;
    const SavedCombination& saved = combinations[slot];
    if (slot == comboIndex) canvas.fillRect(4, y - 2, 232, 14, C_WOOD_DARK);
    canvas.setTextColor(C_WOOD, slot == comboIndex ? C_WOOD_DARK : C_BG);
    if (saved.empty()) {
      canvas.setCursor(8, y); canvas.print(String(slot + 1) + ". " + ui("[VAZIO]", "[EMPTY]"));
    } else {
      String name = saved.name.substring(0, 12);
      String mode = saved.mode == RollMode::Normal ? "" : String(rollModeName(saved.mode, true)) + " ";
      String formula = mode + saved.expression;
      if (formula.length() > 18) formula = formula.substring(0, 17) + "~";
      canvas.setCursor(8, y); canvas.print(String(slot + 1) + ". " + name);
      canvas.setTextColor(C_WOOD_DIM, slot == comboIndex ? C_WOOD_DARK : C_BG);
      canvas.setCursor(232 - formula.length() * 6, y); canvas.print(formula);
    }
  }
  drawFooter(ui("ENT ROLA S SALVA N RENOMEIA M VOLTA", "ENT ROLL S SAVE N RENAME M BACK"));
}

void drawComboName() {
  canvas.fillScreen(C_BG); drawHeader(ui("NOME DA COMBINACAO", "COMBINATION NAME"));
  canvas.setTextColor(C_WOOD_DIM, C_BG); canvas.setCursor(8, 31);
  String summary = comboSaveExpression;
  if (comboSaveMode != RollMode::Normal) summary += " [" + String(rollModeName(comboSaveMode, true)) + "]";
  if (summary.length() > 36) summary = summary.substring(0, 35) + "~";
  canvas.print(summary);
  canvas.drawRoundRect(8, 51, 224, 34, 4, C_WOOD_DIM);
  canvas.setTextColor(C_CREAM, C_BG); canvas.setTextSize(2);
  String shown = comboNameBuffer;
  if ((millis() / 400) & 1) shown += "_";
  canvas.setCursor(15, 61); canvas.print(shown); canvas.setTextSize(1);
  canvas.setTextColor(C_WOOD_DIM, C_BG); canvas.setCursor(29, 96);
  canvas.print(ui("DIGITE ATE 18 CARACTERES", "TYPE UP TO 18 CHARACTERS"));
  drawFooter(ui("ENTER SALVA  DEL APAGA  M CANCELA", "ENTER SAVE  DEL ERASE  M CANCEL"));
}

void drawHelp() {
  canvas.fillScreen(C_BG); drawHeader(String(ui("INSTRUCOES ", "INSTRUCTIONS ")) + String(helpPage + 1) + "/4");
  canvas.setTextColor(C_WOOD, C_BG); canvas.setCursor(8, 29);
  if (helpPage == 0) canvas.print(ui("ROLAGEM RAPIDA\n\nENTER  rolar atual\nR      montar rolagem guiada\nC      combinacoes salvas\nH      historico\nM      menu / voltar", "QUICK ROLL\n\nENTER  roll current\nR      guided roll builder\nC      saved combinations\nH      session history\nM      menu / back"));
  else if (helpPage == 1) canvas.print(ui("NAVEGACAO\n\nSETAS  mover selecao\n[ ]    alterar valor\nENTER  avancar / confirmar\nDEL    apagar\nM      voltar", "NAVIGATION\n\nARROWS move selection\n[ ]    change value\nENTER  next / confirm\nDEL    clear\nM      back"));
  else if (helpPage == 2) canvas.print(ui("EXEMPLOS\n\n2D20+1  dois d20, bonus +1\n3D6-2   tres d6, penalidade -2\n1D20+1D8+5 combinacao\n\nMovimento nunca muda o RNG.", "EXAMPLES\n\n2D20+1  two d20, bonus +1\n3D6-2   three d6, penalty -2\n1D20+1D8+5 combination\n\nMotion never changes RNG."));
  else canvas.print(ui("CAMPANHA\n\nNo assistente, 1D20 permite\nNORMAL, VANTAGEM ou DESVANTAGEM.\nC abre rolagens salvas por nome.\nH abre o historico persistente.\nENTER repete a rolagem escolhida.", "CAMPAIGN\n\nIn the builder, 1D20 supports\nNORMAL, ADVANTAGE or DISADVANTAGE.\nC opens named saved rolls.\nH opens persistent history.\nENTER repeats the selected roll."));
  drawFooter(ui("ESQ/DIR PAGINA  M VOLTA", "LEFT/RIGHT PAGE  M BACK"));
}

void drawAbout() {
  canvas.fillScreen(C_BG); drawHeader(ui("SOBRE", "ABOUT"));
  auto centered = [](const String& text, int y, uint16_t color) {
    canvas.setTextColor(color, C_BG); canvas.setCursor(120 - text.length() * 3, y); canvas.print(text);
  };
  centered("DICE\\VESSEL", 25, C_WOOD);
  centered(String("v") + DICE_VESSEL_VERSION, 38, C_WOOD_DIM);
  centered("Concept: Andre Fuentes", 52, C_WOOD);
  centered("@anfuentz", 64, C_WOOD_DIM);
  centered("Vibecoded by Codex", 79, C_WOOD);
  centered("\"we are the music makers", 96, C_WOOD_DIM);
  centered("and the dreamers of dreams\"", 108, C_WOOD_DIM);
  drawFooter(ui("ENTER OU M VOLTA", "ENTER OR M BACK"));
}

void drawMoment() {
  const uint32_t elapsed = millis() - momentStartedAt;
  canvas.fillScreen(C_BG);

  const int sparkX[] = {12, 31, 54, 77, 101, 139, 164, 188, 214, 229, 22, 69, 176, 220};
  const int sparkY[] = {18, 91, 43, 112, 17, 102, 29, 88, 48, 116, 61, 75, 63, 24};
  for (uint8_t i = 0; i < 14; ++i) {
    const bool bright = ((elapsed / 110 + i * 3) % 7) < 2;
    canvas.drawPixel(sparkX[i], sparkY[i], bright ? C_CREAM : C_WOOD_DARK);
    if (bright && (i % 4 == 0)) canvas.drawPixel(sparkX[i] + 1, sparkY[i], C_WOOD_DIM);
  }

  float progress = min(1.0f, elapsed / 900.0f);
  progress = 1.0f - powf(1.0f - progress, 3.0f);
  int cx = -28 + static_cast<int>(148.0f * progress);
  int cy = 61;
  if (elapsed < 1200) cy -= static_cast<int>(fabsf(sinf(elapsed * 0.0105f)) * (18.0f * (1.0f - min(1.0f, elapsed / 1200.0f))));
  const int wobble = elapsed < 1350 ? static_cast<int>((elapsed / 70) % 3) - 1 : 0;
  const int r = 25;
  int16_t px[6] = {static_cast<int16_t>(cx), static_cast<int16_t>(cx + 21 + wobble), static_cast<int16_t>(cx + 18), static_cast<int16_t>(cx), static_cast<int16_t>(cx - 18), static_cast<int16_t>(cx - 21 - wobble)};
  int16_t py[6] = {static_cast<int16_t>(cy - r), static_cast<int16_t>(cy - 11), static_cast<int16_t>(cy + 15), static_cast<int16_t>(cy + r), static_cast<int16_t>(cy + 15), static_cast<int16_t>(cy - 11)};

  canvas.fillTriangle(px[0], py[0], px[1], py[1], cx, cy, C_WOOD);
  canvas.fillTriangle(px[1], py[1], px[2], py[2], cx, cy, C_WOOD_DIM);
  canvas.fillTriangle(px[2], py[2], px[3], py[3], cx, cy, C_WOOD_DARK);
  canvas.fillTriangle(px[3], py[3], px[4], py[4], cx, cy, C_WOOD_DIM);
  canvas.fillTriangle(px[4], py[4], px[5], py[5], cx, cy, C_WOOD_DARK);
  canvas.fillTriangle(px[5], py[5], px[0], py[0], cx, cy, C_WOOD_DIM);
  for (uint8_t i = 0; i < 6; ++i) {
    canvas.drawLine(px[i], py[i], px[(i + 1) % 6], py[(i + 1) % 6], C_CREAM);
    canvas.drawLine(px[i], py[i], cx, cy, C_WOOD_DARK);
  }

  if (elapsed > 1050) {
    canvas.setTextColor(C_CREAM, C_WOOD);
    canvas.setTextSize(2);
    canvas.setCursor(cx - 12, cy - 7);
    canvas.print("20");
    canvas.setTextSize(1);
  }
  if (elapsed > 1550) {
    const char encoded[] = {84, 72, 69, 32, 68, 73, 67, 69, 32, 82, 69, 77, 69, 77, 66, 69, 82, 46, 0};
    const String message(encoded);
    canvas.setTextColor(C_WOOD, C_BG);
    canvas.setCursor(120 - message.length() * 3, 101);
    canvas.print(message);
  }
  if (elapsed > 2300) drawFooter(ui("ENTER OU M VOLTA", "ENTER OR M BACK"));
}

void drawCharge() {
  canvas.fillScreen(C_BG);
  int level = M5Cardputer.Power.getBatteryLevel();
  level = constrain(level, 0, 100);
  canvas.setTextColor(C_WOOD, C_BG); canvas.setTextSize(2);
  canvas.setCursor(48, 18); canvas.print("DICE\\VESSEL");
  canvas.drawRoundRect(30, 57, 174, 38, 6, C_CREAM);
  canvas.fillRect(204, 67, 7, 18, C_CREAM);
  canvas.fillRoundRect(34, 61, 166 * level / 100, 30, 3, C_GREEN);
  if (M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging) {
    int pulse = 34 + ((millis() / 90) % max(1, 166 * max(level, 5) / 100));
    canvas.drawFastVLine(pulse, 63, 26, C_BG);
    canvas.setTextColor(C_WOOD, C_BG); canvas.setCursor(112, 45); canvas.print(ui("+ CARGA", "+ CHARGING"));
  }
  canvas.setTextColor(C_CREAM, C_BG); canvas.setCursor(92, 105); canvas.print(String(level) + "%");
  drawFooter(ui("ENTER OU M PARA USAR", "ENTER OR M TO USE"));
}

void drawToastOverlay() {
  if (static_cast<int32_t>(toastUntil - millis()) <= 0) return;
  String shown = toast;
  if (shown.length() > 32) shown = shown.substring(0, 31) + "~";
  canvas.fillRoundRect(14, 50, 212, 28, 4, C_PANEL);
  canvas.drawRoundRect(14, 50, 212, 28, 4, C_WOOD);
  canvas.setTextColor(C_CREAM, C_PANEL);
  canvas.setCursor(120 - shown.length() * 3, 60); canvas.print(shown);
}

void draw() {
  if (screen == Screen::Roller) drawRoller();
  else if (screen == Screen::Wizard) drawWizard();
  else if (screen == Screen::Combos) drawCombos();
  else if (screen == Screen::ComboName) drawComboName();
  else if (screen == Screen::Options) drawOptions();
  else if (screen == Screen::History) drawHistory();
  else if (screen == Screen::Help) drawHelp();
  else if (screen == Screen::About) drawAbout();
  else if (screen == Screen::Moment) drawMoment();
  else drawCharge();
  drawToastOverlay();
  canvas.pushSprite(0, 0);
}

bool hasHid(const Keyboard_Class::KeysState& keys, uint8_t code) {
  for (uint8_t key : keys.hid_keys) if (key == code) return true;
  return false;
}

bool hasChar(const Keyboard_Class::KeysState& keys, char target) {
  for (char raw : keys.word) if (toupper(static_cast<unsigned char>(raw)) == toupper(static_cast<unsigned char>(target))) return true;
  return false;
}

void navigationKeys(const Keyboard_Class::KeysState& keys, bool& left, bool& right, bool& up, bool& down, bool& back) {
  left = hasHid(keys, 0x50); right = hasHid(keys, 0x4F);
  down = hasHid(keys, 0x51); up = hasHid(keys, 0x52);
  back = hasHid(keys, 0x29) || hasChar(keys, '`') || (!keys.fn && hasChar(keys, 'M'));
  if (keys.fn) {
    up |= hasChar(keys, 'L'); down |= hasChar(keys, 'M'); left |= hasChar(keys, 'N');
    right |= hasChar(keys, ',') || hasChar(keys, '.') || hasChar(keys, '/');
  }
}

void adjustOption(int delta) {
  bool changed = true;
  if (optionTab == 0) {
    settings.volume = constrain(static_cast<int>(settings.volume) + delta, 0, 10);
    M5Cardputer.Speaker.setVolume(min(255, settings.volume * 29));
  } else if (optionTab == 1 && optionRow == 0) settings.shakeEnabled = !settings.shakeEnabled;
  else if (optionTab == 1 && optionRow == 1) settings.shakeSensitivity = constrain(static_cast<int>(settings.shakeSensitivity) + delta, 0, 10);
  else if (optionTab == 1 && optionRow == 2) settings.clickEnabled = !settings.clickEnabled;
  else if (optionTab == 2 && optionRow == 0) {
    int current = settings.brightness <= 16 ? 0 : constrain((settings.brightness + 12) / 25, 1, 10);
    int value = constrain(current + delta, 0, 10);
    settings.brightness = value == 0 ? 16 : value * 25;
    M5Cardputer.Display.setBrightness(settings.brightness);
  } else if (optionTab == 2 && optionRow == 1) {
    settings.accentColor = (settings.accentColor + delta + ACCENT_COLOR_COUNT) % ACCENT_COLOR_COUNT;
    applyAccentPalette();
  } else if (optionTab == 2 && optionRow == 2) settings.chargeMode = !settings.chargeMode;
  else if (optionTab == 3 && optionRow == 0) settings.portuguese = !settings.portuguese;
  else changed = false;
  if (changed) queueSettingsSave();
}

void handleRollerKeys(const Keyboard_Class::KeysState& keys) {
  bool left, right, up, down, back;
  navigationKeys(keys, left, right, up, down, back);
  if (back) { screen = Screen::Options; return; }
  if (keys.tab) {
    guidedMode = true;
    if (fieldCount) guidedField = (guidedField + 1) % fieldCount;
    typingFresh = false;
    return;
  }
  if (guidedMode && fieldCount) {
    if (left) guidedField = (guidedField + fieldCount - 1) % fieldCount;
    if (right) guidedField = (guidedField + 1) % fieldCount;
    if (up) replaceField(1);
    if (down) replaceField(-1);
  }
  if (keys.del && phase != Phase::Shaking && phase != Phase::Decaying) {
    if (!expression.isEmpty()) expression.remove(expression.length() - 1);
    typingFresh = false; refreshSpec();
  }
  if (keys.fn) return;
  for (char raw : keys.word) {
    char c = static_cast<char>(toupper(static_cast<unsigned char>(raw)));
    if (c == '[') { replaceField(-1); typingFresh = false; return; }
    if (c == ']') { replaceField(1); typingFresh = false; return; }
    if (c == 'R') {
      wizardTermCount = 0; wizardStep = 0; wizardReviewRow = 0;
      wizardSides = 20; wizardQty = 1; wizardModifier = 0; wizardMode = RollMode::Normal;
      screen = Screen::Wizard; return;
    }
    if (c == 'C') { comboIndex = 0; screen = Screen::Combos; return; }
    if (c == 'H') { screen = Screen::History; return; }
    if (c == 'S') { if (spec.valid) { storage.saveQuickRoll(expression); showToast(ui("COMBINACAO SALVA", "COMBINATION SAVED")); } return; }
    if (c == 'L') { expression = storage.loadQuickRoll(); phase = Phase::Idle; refreshSpec(); showToast(ui("COMBINACAO CARREGADA", "COMBINATION LOADED")); return; }
    if (phase == Phase::Shaking || phase == Phase::Decaying) continue;
    if (isdigit(static_cast<unsigned char>(c)) || c == 'D' || c == '+' || c == '-') {
      if (typingFresh) { expression = ""; typingFresh = false; }
      if (expression.length() < 30) expression += c;
      phase = Phase::Idle; refreshSpec();
    }
  }
  if (keys.enter) clickRoll();
}

void handleKeys() {
  if (!(M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed())) return;
  lastInputAt = millis();
  const auto keys = M5Cardputer.Keyboard.keysState();
  bool left, right, up, down, back;
  navigationKeys(keys, left, right, up, down, back);
  if (!keys.fn) {
    up |= hasChar(keys, ';'); down |= hasChar(keys, '.');
    left |= hasChar(keys, ','); right |= hasChar(keys, '/');
  }

  if (screen == Screen::Charge) { if (back || keys.enter) { screen = Screen::Roller; chargeDismissedAt = millis(); } return; }
  if (screen == Screen::Roller) { handleRollerKeys(keys); return; }
  if (screen == Screen::ComboName) {
    if (back) { screen = Screen::Combos; return; }
    if (keys.del && !comboNameBuffer.isEmpty()) comboNameBuffer.remove(comboNameBuffer.length() - 1);
    if (!keys.fn) {
      for (char raw : keys.word) {
        char c = static_cast<char>(toupper(static_cast<unsigned char>(raw)));
        if ((isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') && comboNameBuffer.length() < 18) comboNameBuffer += c;
      }
      if (keys.space && !comboNameBuffer.isEmpty() && comboNameBuffer.length() < 18 && !comboNameBuffer.endsWith(" ")) comboNameBuffer += ' ';
    }
    if (keys.enter) {
      comboNameBuffer.trim();
      if (comboNameBuffer.isEmpty()) comboNameBuffer = "ROLL " + String(comboSaveSlot + 1);
      SavedCombination saved;
      saved.name = comboNameBuffer; saved.expression = comboSaveExpression; saved.mode = comboSaveMode;
      storage.saveCombination(comboSaveSlot, saved);
      combinations[comboSaveSlot] = saved;
      comboIndex = comboSaveSlot; screen = Screen::Combos;
    }
    return;
  }
  if (screen == Screen::Wizard) {
    int delta = hasChar(keys, ']') || right ? 1 : (hasChar(keys, '[') || left ? -1 : 0);
    if (back) {
      if (wizardStep == 0) { if (wizardTermCount) --wizardTermCount; else screen = Screen::Roller; }
      else --wizardStep;
      return;
    }
    if (wizardStep == 0 && delta) {
      const int sides[] = {2,4,6,8,10,12,20,100}; int ix = 0;
      while (ix < 8 && sides[ix] != wizardSides) ++ix;
      wizardSides = sides[(ix + delta + 8) % 8];
    } else if (wizardStep == 1 && delta) wizardQty = constrain(static_cast<int>(wizardQty) + delta, 1, 64);
    else if (wizardStep == 2 && delta) wizardModifier = constrain(static_cast<int>(wizardModifier) + delta, -99, 99);
    else if (wizardStep == 3 && delta) {
      const bool eligible = wizardTermCount == 0 && wizardQty == 1 && wizardSides == 20;
      if (eligible) wizardMode = static_cast<RollMode>((static_cast<int>(wizardMode) + delta + 3) % 3);
      else wizardMode = RollMode::Normal;
    } else if (wizardStep == 4) {
      if (up) wizardReviewRow = (wizardReviewRow + 3) % 4;
      if (down) wizardReviewRow = (wizardReviewRow + 1) % 4;
    }
    if (keys.enter) {
      if (wizardStep < 3) ++wizardStep;
      else if (wizardStep == 3) {
        wizardTerms[wizardTermCount].sign = 1; wizardTerms[wizardTermCount].count = wizardQty; wizardTerms[wizardTermCount].sides = wizardSides;
        wizardStep = 4;
      } else if (wizardReviewRow == 0) {
        expression = wizardExpression(); rollMode = wizardMode; screen = Screen::Roller; phase = Phase::Idle; refreshSpec(); clickRoll();
      } else if (wizardReviewRow == 1 && wizardTermCount < 3) {
        ++wizardTermCount; wizardSides = 6; wizardQty = 1; wizardMode = RollMode::Normal; wizardStep = 0;
      } else if (wizardReviewRow == 2) {
        expression = wizardExpression(); rollMode = wizardMode; refreshSpec();
        uint8_t slot = 0; while (slot < 8 && !combinations[slot].empty()) ++slot;
        if (slot >= 8) slot = comboIndex;
        beginCombinationName(slot, expression, rollMode);
      } else if (wizardReviewRow == 3) screen = Screen::Roller;
    }
    return;
  }
  if (screen == Screen::Combos) {
    if (back) { screen = Screen::Roller; return; }
    if (up) comboIndex = (comboIndex + 7) % 8;
    if (down) comboIndex = (comboIndex + 1) % 8;
    if (keys.del) {
      if (confirmDestructive(ConfirmAction::DeleteCombination, comboIndex, ui("DEL NOVAMENTE PARA APAGAR", "PRESS DEL AGAIN TO DELETE"))) {
        storage.clearCombination(comboIndex); combinations[comboIndex] = SavedCombination();
        showToast(ui("COMBINACAO APAGADA", "COMBINATION DELETED"));
      }
      return;
    }
    if (hasChar(keys, 'S') && spec.valid) {
      SavedCombination current = combinations[comboIndex];
      beginCombinationName(comboIndex, expression, rollMode, current.name); return;
    }
    if (hasChar(keys, 'N')) {
      SavedCombination saved = combinations[comboIndex];
      if (!saved.empty()) beginCombinationName(comboIndex, saved.expression, saved.mode, saved.name);
      return;
    }
    if (keys.enter) {
      SavedCombination saved = combinations[comboIndex];
      if (!saved.empty()) { expression = saved.expression; rollMode = saved.mode; screen = Screen::Roller; phase = Phase::Idle; refreshSpec(); clickRoll(); }
    }
    return;
  }
  if (screen == Screen::Help) {
    if (back) { screen = Screen::Options; return; }
    if (left) helpPage = (helpPage + 3) % 4;
    if (right || keys.enter) helpPage = (helpPage + 1) % 4;
    return;
  }
  if (screen == Screen::About) {
    if (back || keys.enter) { aboutCode = ""; screen = Screen::Options; return; }
    if (millis() - aboutCodeAt > 2500) aboutCode = "";
    if (!keys.fn) {
      for (char raw : keys.word) {
        char c = static_cast<char>(toupper(static_cast<unsigned char>(raw)));
        if (!isalnum(static_cast<unsigned char>(c))) continue;
        aboutCode += c;
        if (aboutCode.length() > 5) aboutCode.remove(0, aboutCode.length() - 5);
        aboutCodeAt = millis();
        uint32_t signature = 2166136261u;
        for (uint8_t i = 0; i < aboutCode.length(); ++i) {
          signature ^= static_cast<uint8_t>(aboutCode[i]);
          signature *= 16777619u;
        }
        if (signature == 0x37A7F87Eu) { beginMoment(); return; }
      }
    }
    return;
  }
  if (screen == Screen::Moment) {
    if (back || keys.enter) screen = Screen::About;
    return;
  }
  if (screen == Screen::Options) {
    if (back) { screen = Screen::Roller; return; }
    if (left) { optionTab = (optionTab + 3) % 4; optionRow = 0; }
    if (right) { optionTab = (optionTab + 1) % 4; optionRow = 0; }
    uint8_t rows = optionTab == 0 ? 1 : (optionTab == 2 ? 3 : (optionTab == 3 ? 5 : 3));
    if (up) optionRow = (optionRow + rows - 1) % rows;
    if (down) optionRow = (optionRow + 1) % rows;
    if (hasChar(keys, '[')) adjustOption(-1);
    if (hasChar(keys, ']')) adjustOption(1);
    if (keys.enter) {
      if (optionTab == 3 && optionRow == 1) { helpPage = 0; screen = Screen::Help; }
      else if (optionTab == 3 && optionRow == 2) screen = Screen::About;
      else if (optionTab == 3 && optionRow == 3) screen = Screen::Charge;
      else if (optionTab == 3 && optionRow == 4) {
        if (confirmDestructive(ConfirmAction::ResetSettings, 0, ui("ENTER NOVAMENTE PARA RESTAURAR", "PRESS ENTER AGAIN TO RESTORE"))) {
          const bool wasPortuguese = settings.portuguese;
          settings = Settings();
          applyAccentPalette();
          M5Cardputer.Display.setBrightness(settings.brightness);
          M5Cardputer.Speaker.setVolume(min(255, settings.volume * 29));
          storage.saveSettings(settings); settingsDirty = false;
          showToast(wasPortuguese ? "PADROES RESTAURADOS" : "DEFAULTS RESTORED");
        }
      }
      else adjustOption(1);
    }
    return;
  }
  if (screen == Screen::History) {
    if (back) { screen = Screen::Roller; return; }
    if (keys.del) {
      if (confirmDestructive(ConfirmAction::ClearHistory, 0, ui("DEL NOVAMENTE PARA LIMPAR", "PRESS DEL AGAIN TO CLEAR"))) {
        historyCount = 0; historyIndex = 0; historyDirty = false;
        storage.saveHistory(history, 0); showToast(ui("HISTORICO LIMPO", "HISTORY CLEARED"));
      }
      return;
    }
    if (historyCount && up) historyIndex = (historyIndex + historyCount - 1) % historyCount;
    if (historyCount && down) historyIndex = (historyIndex + 1) % historyCount;
    if (historyCount && keys.enter) {
      expression = history[historyIndex].expression; rollMode = history[historyIndex].mode; screen = Screen::Roller;
      phase = Phase::Idle; refreshSpec(); clickRoll();
    }
  }
}

void updateMotion() {
  if (clickSequence) return;
  if (!imuAvailable || !settings.shakeEnabled || screen != Screen::Roller) return;
  if (phase == Phase::Decaying) return;
  float ax, ay, az;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;
  float magnitude = sqrtf(ax * ax + ay * ay + az * az);
  float impulse = fabsf(magnitude - 1.0f);
  shakeEnergy = shakeEnergy * 0.82f + impulse * 0.18f;
  float threshold = 0.72f - settings.shakeSensitivity * 0.045f;
  if (shakeEnergy > threshold) {
    lastMotionAt = millis();
    if (phase == Phase::Idle || phase == Phase::Result) {
      refreshSpec(false);
      if (!spec.valid) return;
      physics.configure(spec, 240, 23, 118);
      phase = Phase::Shaking; phaseStartedAt = millis();
    }
    physics.excite(constrain(shakeEnergy * 1.35f, 0.45f, 3.2f));
  }
  if (phase == Phase::Shaking && millis() - lastMotionAt > 260 && millis() - phaseStartedAt > 320) releaseRoll();
}

void updateClickSequence() {
  if (!clickSequence || phase != Phase::Shaking) return;
  const uint32_t now = millis();
  if (now >= clickReleaseAt) { releaseRoll(); return; }
  if (now >= nextClickKickAt) {
    const float pulse = 1.75f + 0.55f * sinf((now - phaseStartedAt) * 0.021f);
    physics.excite(pulse);
    nextClickKickAt = now + 82;
  }
}

void updateCharging() {
  if (!settings.chargeMode || screen != Screen::Roller || phase == Phase::Shaking || phase == Phase::Decaying) return;
  if (millis() - lastInputAt < 15000 || millis() - chargeDismissedAt < 60000) return;
  if (M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging) screen = Screen::Charge;
}

void updatePersistence() {
  const uint32_t now = millis();
  if (settingsDirty && static_cast<int32_t>(now - settingsSaveAt) >= 0) {
    storage.saveSettings(settings);
    settingsDirty = false;
  }
  if (historyDirty && static_cast<int32_t>(now - historySaveAt) >= 0) {
    storage.saveHistory(history, historyCount);
    historyDirty = false;
  }
  if (confirmAction != ConfirmAction::None && static_cast<int32_t>(now - confirmUntil) >= 0) confirmAction = ConfirmAction::None;
}

}  // namespace

void setup() {
  auto config = M5.config();
  config.clear_display = true;
  config.output_power = true;
  M5Cardputer.begin(config, true);
  M5Cardputer.Display.setRotation(1);
  storage.begin();
  settings = storage.loadSettings();
  historyCount = storage.loadHistory(history, 10);
  for (uint8_t i = 0; i < 8; ++i) combinations[i] = storage.loadCombination(i);
  applyAccentPalette();
  M5Cardputer.Display.setBrightness(settings.brightness);
  M5Cardputer.Speaker.setVolume(settings.volume * 24);
  canvas.setColorDepth(16);
  canvas.createSprite(240, 135);
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  drawSplash();
  if (!M5.Imu.isEnabled()) M5.Imu.begin(&M5Cardputer.In_I2C);
  imuAvailable = M5.Imu.isEnabled();
  refreshSpec();
  lastFrameAt = lastDrawAt = lastInputAt = millis();
  toastUntil = 0;
}

void loop() {
  M5Cardputer.update();
  handleKeys();
  updateClickSequence();
  updateMotion();
  updatePersistence();

  uint32_t now = millis();
  float dt = (now - lastFrameAt) / 1000.0f;
  lastFrameAt = now;
  if (phase == Phase::Shaking || phase == Phase::Decaying) {
    physics.update(dt, now);
    woodImpact(physics.consumeImpact());
    if (phase == Phase::Decaying && physics.allSettled()) finishRoll();
  }
  updateCharging();

  if (now - lastDrawAt >= 33) {
    lastDrawAt = now;
    draw();
  }
  delay(1);
}
