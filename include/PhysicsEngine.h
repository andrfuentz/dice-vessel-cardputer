#pragma once

#include "DiceModel.h"

namespace dicevessel {

struct Body {
  float x = 0, y = 0;
  float vx = 0, vy = 0;
  float angle = 0, omega = 0;
  float radius = 9;
  uint16_t sides = 20;
  uint16_t result = 0;
  bool settled = false;
  uint32_t revealAt = 0;
};

class PhysicsEngine {
 public:
  void configure(const RollSpec& spec, int width, int top, int bottom);
  void excite(float energy);
  void beginDecay(const RollResult& result, uint32_t now);
  void update(float dt, uint32_t now);
  bool allSettled() const;
  Body& body(uint8_t index) { return bodies_[index]; }
  const Body& body(uint8_t index) const { return bodies_[index]; }
  uint8_t count() const { return count_; }
  uint8_t hiddenCount() const { return hidden_; }
  bool decaying() const { return decaying_; }
  float consumeImpact();

 private:
  Body bodies_[MAX_BODIES];
  uint8_t count_ = 0;
  uint8_t hidden_ = 0;
  int width_ = 240, top_ = 22, bottom_ = 119;
  bool decaying_ = false;
  bool coinMode_ = false;
  float impact_ = 0;
  void solveWalls(Body& b);
  void solvePairs();
};

}  // namespace dicevessel
