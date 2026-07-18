#pragma once

#include "DiceModel.h"

namespace dicevessel {

class ExpressionParser {
 public:
  static RollSpec parse(const String& input);
  static String normalize(const String& input);
  static bool isSupportedSides(int sides);
};

}  // namespace dicevessel
