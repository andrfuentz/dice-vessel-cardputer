#pragma once

#include <Arduino.h>

namespace dicevessel {

constexpr uint8_t MAX_TERMS = 12;
constexpr uint8_t MAX_RESULTS = 64;
constexpr uint8_t MAX_BODIES = 16;

struct DiceTerm {
  int8_t sign = 1;
  uint8_t count = 1;
  uint16_t sides = 20;
};

struct RollSpec {
  DiceTerm terms[MAX_TERMS];
  uint8_t termCount = 0;
  int16_t modifier = 0;
  uint8_t diceCount = 0;
  bool valid = false;
  String error;
};

struct DieResult {
  uint16_t sides = 20;
  uint16_t value = 1;
  int8_t sign = 1;
};

struct RollResult {
  DieResult dice[MAX_RESULTS];
  uint8_t count = 0;
  int32_t total = 0;
  String expression;
};

struct HistoryEntry {
  String expression;
  int32_t total = 0;
  uint32_t timestampMs = 0;
};

}  // namespace dicevessel
