#include "ExpressionParser.h"

#include <ctype.h>

namespace dicevessel {

bool ExpressionParser::isSupportedSides(int sides) {
  static const int allowed[] = {2, 4, 6, 8, 10, 12, 20, 100};
  for (int value : allowed) if (sides == value) return true;
  return false;
}

String ExpressionParser::normalize(const String& input) {
  String out;
  out.reserve(input.length());
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];
    if (c == ' ' || c == '\t') continue;
    out += static_cast<char>(toupper(static_cast<unsigned char>(c)));
  }
  return out;
}

RollSpec ExpressionParser::parse(const String& input) {
  RollSpec spec;
  const String text = normalize(input);
  if (text.isEmpty()) { spec.error = "EXPRESSAO VAZIA"; return spec; }

  size_t i = 0;
  int sign = 1;
  bool sawDice = false;
  while (i < text.length()) {
    if (text[i] == '+' || text[i] == '-') {
      sign = text[i] == '-' ? -1 : 1;
      if (++i >= text.length()) { spec.error = "TERMO INCOMPLETO"; return spec; }
    }

    if (!isdigit(static_cast<unsigned char>(text[i]))) {
      spec.error = "NUMERO ESPERADO";
      return spec;
    }
    long number = 0;
    while (i < text.length() && isdigit(static_cast<unsigned char>(text[i]))) {
      number = number * 10 + (text[i++] - '0');
      if (number > 9999) { spec.error = "NUMERO MUITO GRANDE"; return spec; }
    }

    if (i < text.length() && text[i] == 'D') {
      ++i;
      if (number < 1 || number > MAX_RESULTS) { spec.error = "MAXIMO 64 DADOS"; return spec; }
      if (i >= text.length() || !isdigit(static_cast<unsigned char>(text[i]))) {
        spec.error = "FACES ESPERADAS";
        return spec;
      }
      long sides = 0;
      while (i < text.length() && isdigit(static_cast<unsigned char>(text[i]))) {
        sides = sides * 10 + (text[i++] - '0');
      }
      if (!isSupportedSides(sides)) { spec.error = "DADO NAO SUPORTADO"; return spec; }
      if (spec.termCount >= MAX_TERMS) { spec.error = "MUITOS TERMOS"; return spec; }
      if (spec.diceCount + number > MAX_RESULTS) { spec.error = "MAXIMO 64 DADOS"; return spec; }
      auto& term = spec.terms[spec.termCount++];
      term.sign = sign;
      term.count = static_cast<uint8_t>(number);
      term.sides = static_cast<uint16_t>(sides);
      spec.diceCount += term.count;
      sawDice = true;
    } else {
      long next = static_cast<long>(spec.modifier) + sign * number;
      if (next < -9999 || next > 9999) { spec.error = "MODIFICADOR INVALIDO"; return spec; }
      spec.modifier = static_cast<int16_t>(next);
    }

    sign = 1;
    if (i < text.length() && text[i] != '+' && text[i] != '-') {
      spec.error = "CARACTERE INVALIDO";
      return spec;
    }
  }

  if (!sawDice) { spec.error = "ADICIONE UM DADO"; return spec; }
  spec.valid = true;
  return spec;
}

}  // namespace dicevessel
