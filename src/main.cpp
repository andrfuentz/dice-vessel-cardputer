#include <M5Cardputer.h>
#include <M5Unified.h>
#include <esp_system.h>
#include <math.h>

#include "DiceModel.h"
#include "ExpressionParser.h"
#include "PhysicsEngine.h"
#include "Storage.h"

using namespace dicevessel;

namespace {

constexpr uint16_t C_BG = 0x0000;
constexpr uint16_t C_PANEL = 0x0000;
constexpr uint16_t C_WOOD = 0xFD20;
constexpr uint16_t C_WOOD_DIM = 0x7A80;
constexpr uint16_t C_WOOD_DARK = 0x3180;
constexpr uint16_t C_CREAM = 0xFD20;
constexpr uint16_t C_RED = 0xF9C7;
constexpr uint16_t C_GREEN = 0xFD20;
constexpr uint16_t C_SHADOW = 0x0000;

enum class Screen { Roller, Wizard, Combos, Options, History, Help, About, Charge };
enum class Phase { Idle, Shaking, Decaying, Result };
enum class ResultEvent { None, Critical, Failure };

M5Canvas canvas(&M5Cardputer.Display);
PhysicsEngine physics;
Storage storage;
Settings settings;
Screen screen = Screen::Roller;
Phase phase = Phase::Idle;
String expression = "1D20";
RollSpec spec;
RollResult lastResult;
HistoryEntry history[10];
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

DiceTerm wizardTerms[4];
uint8_t wizardTermCount = 0;
uint8_t wizardStep = 0;
uint8_t wizardReviewRow = 0;
uint16_t wizardSides = 20;
uint8_t wizardQty = 1;
int16_t wizardModifier = 0;

struct EditField { uint8_t start, end; enum Kind { Count, Sides, Modifier } kind; };
EditField fields[24];
uint8_t fieldCount = 0;

const char* ui(const char* pt, const char* en) {
  return settings.portuguese ? pt : en;
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
  out.total = roll.modifier;
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
  history[0].timestampMs = millis();
  if (historyCount < 10) ++historyCount;
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
  clickReleaseAt = millis() + (spec.diceCount == 1 && spec.terms[0].sides == 2 ? 180 : 620);
  nextClickKickAt = millis() + 105;
  physics.excite(spec.diceCount == 1 && spec.terms[0].sides == 2 ? 1.0f : 2.7f);
  typingFresh = true;
}

void finishRoll() {
  if (phase == Phase::Result) return;
  phase = Phase::Result;
  addHistory(lastResult);
  resultEvent = ResultEvent::None;
  bool hasMax = false, hasOne = false;
  for (uint8_t i = 0; i < lastResult.count; ++i) {
    if (lastResult.dice[i].sides > 2) {
      hasMax |= lastResult.dice[i].value == lastResult.dice[i].sides;
      hasOne |= lastResult.dice[i].value == 1;
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

void drawDieShape(const Body& b, bool reveal) {
  const int x = static_cast<int>(b.x), y = static_cast<int>(b.y);
  const int r = static_cast<int>(b.radius);
  if (!reveal) canvas.drawEllipse(x + 2, y + r - 1, r, 2, C_WOOD_DARK);

  if (b.sides == 2) {
    int width = reveal ? r * 2 : max(2, static_cast<int>(fabsf(cosf(b.angle)) * r * 2));
    if (reveal) canvas.fillEllipse(x, y, width / 2, r, C_WOOD);
    else canvas.drawEllipse(x, y, width / 2, r, C_WOOD);
  } else if (b.sides == 4) {
    if (reveal) canvas.fillTriangle(x, y - r, x - r, y + r, x + r, y + r, C_WOOD);
    canvas.drawTriangle(x, y - r, x - r, y + r, x + r, y + r, C_CREAM);
    if (!reveal) canvas.drawLine(x, y - r, x, y + r, C_WOOD_DARK);
  } else if (b.sides == 6) {
    if (reveal) canvas.fillRect(x - r, y - r, r * 2, r * 2, C_WOOD);
    canvas.drawRect(x - r, y - r, r * 2, r * 2, C_CREAM);
    if (!reveal) canvas.drawLine(x - r, y - r, x - r + 4, y - r - 3, C_WOOD_DIM);
  } else {
    int vertices = b.sides == 8 ? 4 : (b.sides == 10 ? 5 : 6);
    float offset = b.angle;
    for (int i = 0; i < vertices; ++i) {
      float a1 = offset + i * TWO_PI / vertices;
      float a2 = offset + (i + 1) * TWO_PI / vertices;
      if (reveal) canvas.fillTriangle(x, y, x + cosf(a1) * r, y + sinf(a1) * r,
                          x + cosf(a2) * r, y + sinf(a2) * r, C_WOOD);
      canvas.drawLine(x + cosf(a1) * r, y + sinf(a1) * r,
                      x + cosf(a2) * r, y + sinf(a2) * r, C_CREAM);
    }
  }

  if (reveal && b.result) {
    String value = b.sides == 100 && b.result == 100 ? "00" : String(b.result);
    canvas.setTextSize(r >= 16 && value.length() <= 2 ? 2 : 1);
    canvas.setTextColor(C_BG, C_WOOD);
    int charWidth = r >= 16 && value.length() <= 2 ? 12 : 6;
    int charHeight = r >= 16 && value.length() <= 2 ? 8 : 3;
    canvas.setCursor(x - value.length() * charWidth / 2, y - charHeight);
    canvas.print(value);
    canvas.setTextSize(1);
    if (resultEvent != ResultEvent::None) {
      String eventText = resultEvent == ResultEvent::Critical ? ui("CRITICO!", "CRITICAL!") : ui("FALHA!", "FAILURE!");
      canvas.fillRoundRect(78, 25, 84, 18, 3, C_WOOD);
      canvas.setTextColor(C_BG, C_WOOD);
      canvas.setCursor(120 - eventText.length() * 3, 31);
      canvas.print(eventText);
    }
  }
}

void drawRoller() {
  canvas.fillScreen(C_BG);
  drawHeader("DICE\\VESSEL  " + expression);
  canvas.drawFastHLine(0, 22, 240, C_WOOD_DARK);

  for (uint8_t i = 0; i < physics.count(); ++i) {
    const Body& body = physics.body(i);
    drawDieShape(body, body.settled || phase == Phase::Result);
  }
  if (physics.hiddenCount()) {
    canvas.setTextColor(C_CREAM, C_PANEL);
    canvas.fillRoundRect(194, 27, 41, 15, 3, C_PANEL);
    canvas.setCursor(200, 31);
    canvas.print("+" + String(physics.hiddenCount()));
  }

  if (phase == Phase::Idle) {
    canvas.fillRect(0, 118, 240, 17, C_BG);
    canvas.drawFastHLine(0, 117, 240, C_WOOD_DARK);
    canvas.setTextColor(spec.valid ? C_CREAM : C_RED, C_PANEL);
    canvas.setCursor(4, 123);
    canvas.print(spec.valid ? (imuAvailable && settings.shakeEnabled ? "SHAKE / ENTER" : ui("ENTER: ROLAR", "ENTER: ROLL")) : uiError(spec.error));
    canvas.setCursor(145, 123); canvas.print(ui("R NOVA  C SALVAS", "R NEW  C SAVED"));
  } else if (phase == Phase::Shaking) {
    canvas.setTextColor(C_CREAM, C_BG); canvas.setCursor(86, 124); canvas.print(ui("SACUDINDO...", "SHAKING..."));
  } else if (phase == Phase::Decaying) {
    canvas.setTextColor(C_WOOD, C_BG); canvas.setCursor(84, 124); canvas.print(ui("ROLANDO...", "ROLLING..."));
  } else {
    canvas.fillRect(0, 114, 240, 21, C_BG);
    canvas.drawFastHLine(0, 113, 240, C_WOOD_DARK);
    canvas.setTextColor(C_GREEN, C_PANEL); canvas.setTextSize(2);
    String total = String("TOTAL ") + String(lastResult.total);
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

  if (millis() < toastUntil) {
    canvas.fillRoundRect(44, 50, 152, 28, 4, C_PANEL);
    canvas.drawRoundRect(44, 50, 152, 28, 4, C_WOOD);
    canvas.setTextColor(C_CREAM, C_PANEL);
    canvas.setCursor(120 - toast.length() * 3, 60); canvas.print(toast);
  }
}

void drawOptions() {
  canvas.fillScreen(C_BG); drawHeader(ui("OPCOES", "OPTIONS"));
  const char* tabsPt[] = {"AUDIO", "MOV", "TELA", "SISTEMA"};
  const char* tabsEn[] = {"AUDIO", "MOTION", "DISPLAY", "SYSTEM"};
  int tabX[] = {3, 61, 111, 164};
  for (uint8_t i = 0; i < 4; ++i) {
    if (i == optionTab) canvas.fillRect(tabX[i], 23, i == 3 ? 73 : 48, 14, C_WOOD_DARK);
    canvas.setTextColor(C_WOOD, i == optionTab ? C_WOOD_DARK : C_BG);
    canvas.setCursor(tabX[i] + 4, 27); canvas.print(settings.portuguese ? tabsPt[i] : tabsEn[i]);
  }
  canvas.drawFastHLine(0, 39, 240, C_WOOD_DARK);
  auto row = [&](uint8_t index, int y, const String& label, const String& value, int barValue) {
    if (index == optionRow) canvas.fillRect(4, y - 3, 232, 20, C_WOOD_DARK);
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
    int brightness10 = constrain((settings.brightness + 12) / 25, 0, 10);
    row(0, 48, ui("BRILHO", "BRIGHTNESS"), String(brightness10) + "/10", brightness10);
    row(1, 76, ui("MODO CARGA", "CHARGE MODE"), settings.chargeMode ? "ON" : "OFF", -1);
  } else {
    row(0, 45, ui("IDIOMA", "LANGUAGE"), settings.portuguese ? "PORTUGUES" : "ENGLISH", -1);
    row(1, 63, ui("INSTRUCOES", "INSTRUCTIONS"), "ENTER", -1);
    row(2, 81, ui("SOBRE", "ABOUT"), "ENTER", -1);
    row(3, 99, ui("BATERIA", "BATTERY"), "ENTER", -1);
  }
  canvas.drawFastHLine(0, 117, 240, C_WOOD_DARK);
  canvas.setTextColor(C_WOOD_DIM, C_BG); canvas.setCursor(4, 123); canvas.print(ui(",/ ABAS  ;. ITEM  [] AJUSTA", ",/ TABS  ;. ITEM  [] CHANGE"));
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
    canvas.setCursor(8, y); canvas.print(history[i].expression);
    String total = "= " + String(history[i].total);
    canvas.setCursor(232 - total.length() * 6, y); canvas.print(total);
  }
  canvas.setTextColor(C_CREAM, C_BG); canvas.setCursor(5, 124); canvas.print(ui("ENTER REPETE   ` VOLTA", "ENTER REPEAT   ` BACK"));
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
  const char* titlesPt[] = {"1/4 TIPO DE DADO", "2/4 QUANTIDADE", "3/4 BONUS / PENALIDADE", "4/4 CONFIRMAR"};
  const char* titlesEn[] = {"1/4 DIE TYPE", "2/4 QUANTITY", "3/4 BONUS / PENALTY", "4/4 CONFIRM"};
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
  } else {
    canvas.setTextSize(2); String value = wizardExpression();
    canvas.setCursor(120 - value.length() * 6, 27); canvas.print(value); canvas.setTextSize(1);
    const char* actionsPt[] = {"ROLAR AGORA", "+ ADICIONAR DADO", "SALVAR COMBINACAO", "CANCELAR"};
    const char* actionsEn[] = {"ROLL NOW", "+ ADD DIE", "SAVE COMBINATION", "CANCEL"};
    for (uint8_t i = 0; i < 4; ++i) {
      int y = 54 + i * 16;
      if (i == wizardReviewRow) canvas.fillRect(18, y - 2, 204, 14, C_WOOD_DARK);
      canvas.setTextColor(C_WOOD, i == wizardReviewRow ? C_WOOD_DARK : C_BG);
      canvas.setCursor(28, y); canvas.print(settings.portuguese ? actionsPt[i] : actionsEn[i]);
    }
  }
  canvas.drawFastHLine(0, 117, 240, C_WOOD_DARK);
  canvas.setTextColor(C_WOOD_DIM, C_BG); canvas.setCursor(6, 123);
  canvas.print(wizardStep == 3 ? ui(";/. MOVE  ENTER OK  ` VOLTA", ";/. MOVE  ENTER OK  ` BACK") : ui("[ ] AJUSTA  ENTER PROX  ` VOLTA", "[ ] CHANGE  ENTER NEXT  ` BACK"));
}

void drawCombos() {
  canvas.fillScreen(C_BG); drawHeader(ui("COMBINACOES SALVAS", "SAVED COMBINATIONS"));
  uint8_t start = comboIndex >= 6 ? 2 : 0;
  for (uint8_t i = 0; i < 6; ++i) {
    uint8_t slot = start + i;
    int y = 27 + i * 15;
    String saved = storage.loadCombination(slot);
    if (slot == comboIndex) canvas.fillRect(4, y - 2, 232, 14, C_WOOD_DARK);
    canvas.setTextColor(C_WOOD, slot == comboIndex ? C_WOOD_DARK : C_BG);
    canvas.setCursor(8, y); canvas.print(String(slot + 1) + ". " + (saved.isEmpty() ? ui("[VAZIO]", "[EMPTY]") : saved));
  }
  canvas.drawFastHLine(0, 117, 240, C_WOOD_DARK);
  canvas.setTextColor(C_WOOD_DIM, C_BG); canvas.setCursor(4, 123); canvas.print(ui("ENTER ROLA  S SALVA  DEL APAGA", "ENTER ROLL  S SAVE  DEL CLEAR"));
}

void drawHelp() {
  canvas.fillScreen(C_BG); drawHeader(String(ui("INSTRUCOES ", "INSTRUCTIONS ")) + String(helpPage + 1) + "/3");
  canvas.setTextColor(C_WOOD, C_BG); canvas.setCursor(8, 29);
  if (helpPage == 0) canvas.print(ui("ROLAGEM RAPIDA\n\nENTER  rolar atual\nR      montar rolagem guiada\nC      combinacoes salvas\nH      historico\n`      opcoes / voltar", "QUICK ROLL\n\nENTER  roll current\nR      guided roll builder\nC      saved combinations\nH      session history\n`      options / back"));
  else if (helpPage == 1) canvas.print(ui("MONTAR ROLAGEM\n\n[ ]    alterar valor\nENTER  avancar / confirmar\n; .    mover selecao\n, /    trocar aba\nDEL    apagar combinacao", "BUILD A ROLL\n\n[ ]    change value\nENTER  next / confirm\n; .    move selection\n, /    change tab\nDEL    clear combination"));
  else canvas.print(ui("EXEMPLOS\n\n2D20+1  dois d20, bonus +1\n3D6-2   tres d6, penalidade -2\n1D20+1D8+5 combinacao\n\nMovimento nunca muda o RNG.", "EXAMPLES\n\n2D20+1  two d20, bonus +1\n3D6-2   three d6, penalty -2\n1D20+1D8+5 combination\n\nMotion never changes RNG."));
  canvas.setCursor(6, 123); canvas.setTextColor(C_WOOD_DIM, C_BG); canvas.print(ui(", / PAGINA   ` VOLTA", ", / PAGE   ` BACK"));
}

void drawAbout() {
  canvas.fillScreen(C_BG); drawHeader(ui("SOBRE", "ABOUT"));
  auto centered = [](const String& text, int y, uint16_t color) {
    canvas.setTextColor(color, C_BG); canvas.setCursor(120 - text.length() * 3, y); canvas.print(text);
  };
  centered("DICE\\VESSEL", 26, C_WOOD);
  centered("Concept: Andre Fuentes", 42, C_WOOD);
  centered("@anfuentz", 54, C_WOOD_DIM);
  centered("Vibecoded by Codex", 70, C_WOOD);
  centered("\"we are the music", 88, C_WOOD_DIM);
  centered("makers and we are", 100, C_WOOD_DIM);
  centered("the dreamers of dreams\"", 112, C_WOOD_DIM);
  canvas.setCursor(6, 125); canvas.print(ui("` VOLTA", "` BACK"));
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
  canvas.setTextSize(1); canvas.setCursor(63, 125); canvas.print(ui("` PARA USAR", "` TO USE"));
}

void draw() {
  if (screen == Screen::Roller) drawRoller();
  else if (screen == Screen::Wizard) drawWizard();
  else if (screen == Screen::Combos) drawCombos();
  else if (screen == Screen::Options) drawOptions();
  else if (screen == Screen::History) drawHistory();
  else if (screen == Screen::Help) drawHelp();
  else if (screen == Screen::About) drawAbout();
  else drawCharge();
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
  back = hasHid(keys, 0x29) || hasChar(keys, '`');
  if (keys.fn) {
    up |= hasChar(keys, 'L'); down |= hasChar(keys, 'M'); left |= hasChar(keys, 'N');
    right |= hasChar(keys, ',') || hasChar(keys, '.') || hasChar(keys, '/');
  }
}

void adjustOption(int delta) {
  if (optionTab == 0) settings.volume = constrain(static_cast<int>(settings.volume) + delta, 0, 10);
  else if (optionTab == 1 && optionRow == 0) settings.shakeEnabled = !settings.shakeEnabled;
  else if (optionTab == 1 && optionRow == 1) settings.shakeSensitivity = constrain(static_cast<int>(settings.shakeSensitivity) + delta, 0, 10);
  else if (optionTab == 1 && optionRow == 2) settings.clickEnabled = !settings.clickEnabled;
  else if (optionTab == 2 && optionRow == 0) {
    int value = constrain((settings.brightness + 12) / 25 + delta, 0, 10);
    settings.brightness = value == 0 ? 16 : value * 25;
    M5Cardputer.Display.setBrightness(settings.brightness);
  } else if (optionTab == 2 && optionRow == 1) settings.chargeMode = !settings.chargeMode;
  else if (optionTab == 3 && optionRow == 0) settings.portuguese = !settings.portuguese;
  storage.saveSettings(settings);
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
      wizardSides = 20; wizardQty = 1; wizardModifier = 0;
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
    else if (wizardStep == 3) {
      if (up) wizardReviewRow = (wizardReviewRow + 3) % 4;
      if (down) wizardReviewRow = (wizardReviewRow + 1) % 4;
    }
    if (keys.enter) {
      if (wizardStep < 2) ++wizardStep;
      else if (wizardStep == 2) {
        wizardTerms[wizardTermCount].sign = 1; wizardTerms[wizardTermCount].count = wizardQty; wizardTerms[wizardTermCount].sides = wizardSides;
        wizardStep = 3;
      } else if (wizardReviewRow == 0) {
        expression = wizardExpression(); screen = Screen::Roller; phase = Phase::Idle; refreshSpec(); clickRoll();
      } else if (wizardReviewRow == 1 && wizardTermCount < 3) {
        ++wizardTermCount; wizardSides = 6; wizardQty = 1; wizardStep = 0;
      } else if (wizardReviewRow == 2) {
        expression = wizardExpression(); refreshSpec();
        uint8_t slot = 0; while (slot < 8 && !storage.loadCombination(slot).isEmpty()) ++slot;
        if (slot >= 8) slot = comboIndex;
        storage.saveCombination(slot, expression); comboIndex = slot; screen = Screen::Combos;
      } else if (wizardReviewRow == 3) screen = Screen::Roller;
    }
    return;
  }
  if (screen == Screen::Combos) {
    if (back) { screen = Screen::Roller; return; }
    if (up) comboIndex = (comboIndex + 7) % 8;
    if (down) comboIndex = (comboIndex + 1) % 8;
    if (keys.del) { storage.clearCombination(comboIndex); return; }
    if (hasChar(keys, 'S') && spec.valid) { storage.saveCombination(comboIndex, expression); showToast(ui("COMBINACAO SALVA", "COMBINATION SAVED")); return; }
    if (keys.enter) {
      String saved = storage.loadCombination(comboIndex);
      if (!saved.isEmpty()) { expression = saved; screen = Screen::Roller; phase = Phase::Idle; refreshSpec(); clickRoll(); }
    }
    return;
  }
  if (screen == Screen::Help) {
    if (back) { screen = Screen::Options; return; }
    if (left) helpPage = (helpPage + 2) % 3;
    if (right || keys.enter) helpPage = (helpPage + 1) % 3;
    return;
  }
  if (screen == Screen::About) { if (back || keys.enter) screen = Screen::Options; return; }
  if (screen == Screen::Options) {
    if (back) { screen = Screen::Roller; return; }
    if (left) { optionTab = (optionTab + 3) % 4; optionRow = 0; }
    if (right) { optionTab = (optionTab + 1) % 4; optionRow = 0; }
    uint8_t rows = optionTab == 0 ? 1 : (optionTab == 2 ? 2 : (optionTab == 3 ? 4 : 3));
    if (up) optionRow = (optionRow + rows - 1) % rows;
    if (down) optionRow = (optionRow + 1) % rows;
    if (hasChar(keys, '[')) adjustOption(-1);
    if (hasChar(keys, ']')) adjustOption(1);
    if (keys.enter) {
      if (optionTab == 3 && optionRow == 1) { helpPage = 0; screen = Screen::Help; }
      else if (optionTab == 3 && optionRow == 2) screen = Screen::About;
      else if (optionTab == 3 && optionRow == 3) screen = Screen::Charge;
      else adjustOption(1);
    }
    return;
  }
  if (screen == Screen::History) {
    if (back) { screen = Screen::Roller; return; }
    if (historyCount && up) historyIndex = (historyIndex + historyCount - 1) % historyCount;
    if (historyCount && down) historyIndex = (historyIndex + 1) % historyCount;
    if (historyCount && keys.enter) {
      expression = history[historyIndex].expression; screen = Screen::Roller;
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
    physics.excite(1.75f);
    nextClickKickAt = now + 105;
  }
}

void updateCharging() {
  if (!settings.chargeMode || screen != Screen::Roller || phase == Phase::Shaking || phase == Phase::Decaying) return;
  if (millis() - lastInputAt < 15000 || millis() - chargeDismissedAt < 60000) return;
  if (M5Cardputer.Power.isCharging() == m5::Power_Class::is_charging) screen = Screen::Charge;
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
