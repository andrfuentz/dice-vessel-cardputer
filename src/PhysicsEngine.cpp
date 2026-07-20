#include "PhysicsEngine.h"

#include <esp_system.h>
#include <math.h>

namespace dicevessel {

static float rnd(float low, float high) {
  return low + (esp_random() / 4294967295.0f) * (high - low);
}

static float speedFor(uint16_t sides) {
  if (sides == 4) return 0.78f;
  if (sides == 10) return 1.10f;
  if (sides == 12) return 0.84f;
  if (sides == 20) return 1.16f;
  return 1.0f;
}

static float impactWeightFor(uint16_t sides) {
  if (sides == 4) return 0.82f;
  if (sides == 12) return 1.28f;
  if (sides == 20) return 1.08f;
  return 1.0f;
}

uint8_t PhysicsEngine::visualCount(const RollSpec& spec) const {
  if (spec.mode != RollMode::Normal) return 2;
  uint16_t total = 0;
  for (uint8_t t = 0; t < spec.termCount; ++t) {
    total += spec.terms[t].count * (spec.terms[t].sides == 100 ? 2 : 1);
  }
  return min<uint16_t>(total, 255);
}

void PhysicsEngine::configure(const RollSpec& spec, int width, int top, int bottom) {
  width_ = width; top_ = top; bottom_ = bottom;
  const uint8_t requestedBodies = visualCount(spec);
  count_ = min<uint8_t>(requestedBodies, MAX_BODIES);
  coinMode_ = spec.diceCount == 1 && spec.termCount == 1 && spec.terms[0].sides == 2;
  hidden_ = requestedBodies > count_ ? requestedBodies - count_ : 0;
  uint8_t index = 0;
  uint8_t resultIndex = 0;
  const float commonRadius = count_ <= 1 ? 24.0f : (count_ <= 4 ? 17.0f : (count_ <= 8 ? 13.0f : 9.0f));
  const uint8_t columns = max<uint8_t>(1, min<uint8_t>(count_, 8));
  const float spacing = width_ / static_cast<float>(columns);
  if (spec.mode != RollMode::Normal) {
    for (index = 0; index < count_; ++index) {
      Body& b = bodies_[index];
      b.sides = 20; b.percentilePart = 0; b.resultIndex = index;
      b.radius = commonRadius;
      b.x = spacing * (index + 0.5f) + rnd(-2, 2);
      b.y = bottom_ - b.radius + rnd(-2, 2);
      b.vx = b.vy = b.omega = 0;
      b.angle = rnd(-0.5f, 0.5f);
      b.result = 0; b.kept = true; b.settled = false; b.revealAt = 0;
    }
  } else for (uint8_t t = 0; t < spec.termCount; ++t) {
    for (uint8_t n = 0; n < spec.terms[t].count; ++n, ++resultIndex) {
      const uint8_t parts = spec.terms[t].sides == 100 ? 2 : 1;
      for (uint8_t part = 0; part < parts && index < count_; ++part, ++index) {
        Body& b = bodies_[index];
        b.sides = spec.terms[t].sides == 100 ? 10 : spec.terms[t].sides;
        b.percentilePart = spec.terms[t].sides == 100 ? part + 1 : 0;
        b.resultIndex = resultIndex;
        b.radius = b.sides == 2 ? commonRadius * 0.82f : commonRadius;
        const uint8_t col = index % columns;
        const uint8_t row = index / columns;
        b.x = spacing * (col + 0.5f) + rnd(-2, 2);
        b.y = bottom_ - b.radius - row * (commonRadius * 1.55f) + rnd(-2, 2);
        b.vx = b.vy = b.omega = 0;
        b.angle = rnd(-0.5f, 0.5f);
        b.result = 0; b.kept = true; b.settled = false; b.revealAt = 0;
        if (coinMode_) { b.x = width_ * 0.5f; b.y = bottom_ - b.radius; }
      }
    }
  }
  decaying_ = false;
}

void PhysicsEngine::excite(float energy) {
  const float e = constrain(energy, 0.25f, 3.5f);
  for (uint8_t i = 0; i < count_; ++i) {
    Body& b = bodies_[i];
    if (coinMode_) {
      b.x = width_ * 0.5f; b.y = top_ + b.radius + 3;
      b.vx = 0; b.vy = 15; b.omega = 20;
      b.settled = false;
      continue;
    }
    const float speed = speedFor(b.sides);
    b.vx += rnd(-105, 105) * e * speed;
    b.vy += rnd(-95, 75) * e * speed;
    b.omega += rnd(-10, 10) * e * (b.sides == 4 ? 0.62f : speed);
    b.settled = false;
  }
}

void PhysicsEngine::beginDecay(const RollResult& result, uint32_t now) {
  decaying_ = true;
  if (count_ && fabsf(bodies_[0].vx) + fabsf(bodies_[0].vy) < 20) excite(0.9f);
  for (uint8_t i = 0; i < count_; ++i) {
    Body& b = bodies_[i];
    const uint16_t value = b.resultIndex < result.count ? result.dice[b.resultIndex].value : 0;
    if (b.percentilePart == 1) b.result = value == 100 ? 0 : (value / 10) * 10;
    else if (b.percentilePart == 2) b.result = value == 100 ? 0 : value % 10;
    else b.result = value;
    b.kept = b.resultIndex < result.count ? result.dice[b.resultIndex].kept : true;
    b.settled = false;
    b.revealAt = now + (coinMode_ ? 900 : 620 + i * 42 + static_cast<uint32_t>(rnd(0, 190)));
  }
}

void PhysicsEngine::solveWalls(Body& b) {
  float bounce = 0.72f;
  if (b.sides == 4) bounce = 0.56f;
  else if (b.sides == 6) bounce = 0.66f;
  else if (b.sides == 10) bounce = 0.77f;
  else if (b.sides == 12) bounce = 0.62f;
  else if (b.sides == 20) bounce = 0.81f;
  const float weight = impactWeightFor(b.sides);
  if (b.x - b.radius < 0) { impact_ = max(impact_, fabsf(b.vx) * weight); b.x = b.radius; b.vx = fabsf(b.vx) * bounce; }
  if (b.x + b.radius >= width_) { impact_ = max(impact_, fabsf(b.vx) * weight); b.x = width_ - b.radius - 1; b.vx = -fabsf(b.vx) * bounce; }
  if (b.y - b.radius < top_) { impact_ = max(impact_, fabsf(b.vy) * weight); b.y = top_ + b.radius; b.vy = fabsf(b.vy) * bounce; }
  if (b.y + b.radius >= bottom_) { impact_ = max(impact_, fabsf(b.vy) * weight); b.y = bottom_ - b.radius - 1; b.vy = -fabsf(b.vy) * bounce; }
}

void PhysicsEngine::solvePairs() {
  for (uint8_t i = 0; i < count_; ++i) for (uint8_t j = i + 1; j < count_; ++j) {
    Body& a = bodies_[i]; Body& b = bodies_[j];
    float dx = b.x - a.x, dy = b.y - a.y;
    float minD = a.radius + b.radius;
    float d2 = dx * dx + dy * dy;
    if (d2 <= 0.01f || d2 >= minD * minD) continue;
    float d = sqrtf(d2), nx = dx / d, ny = dy / d;
    float overlap = (minD - d) * 0.5f;
    a.x -= nx * overlap; a.y -= ny * overlap;
    b.x += nx * overlap; b.y += ny * overlap;
    float rv = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
    if (rv < 0) {
      impact_ = max(impact_, -rv * 0.55f);
      float impulse = -0.78f * rv;
      a.vx -= impulse * nx; a.vy -= impulse * ny;
      b.vx += impulse * nx; b.vy += impulse * ny;
    }
  }
}

void PhysicsEngine::update(float dt, uint32_t now) {
  impact_ = 0;
  dt = constrain(dt, 0.001f, 0.05f);
  for (uint8_t i = 0; i < count_; ++i) {
    Body& b = bodies_[i];
    if (b.settled) continue;
    if (coinMode_) b.x = width_ * 0.5f;
    float decayBase = 0.095f;
    if (b.sides == 4) decayBase = 0.055f;
    else if (b.sides == 10) decayBase = 0.13f;
    else if (b.sides == 12) decayBase = 0.07f;
    else if (b.sides == 20) decayBase = 0.15f;
    const float drag = decaying_ ? powf(decayBase, dt) : powf(0.86f, dt);
    b.vy += (b.sides == 12 ? 36.0f : 30.0f) * dt;
    b.x += b.vx * dt; b.y += b.vy * dt;
    b.angle += b.omega * dt;
    b.vx *= drag; b.vy *= drag; b.omega *= drag;
    solveWalls(b);
    if (decaying_ && now >= b.revealAt && fabsf(b.vx) + fabsf(b.vy) < 24.0f) {
      b.vx = b.vy = b.omega = 0; b.settled = true;
    }
  }
  solvePairs();
}

float PhysicsEngine::consumeImpact() {
  const float value = impact_;
  impact_ = 0;
  return value;
}

bool PhysicsEngine::allSettled() const {
  if (!decaying_ || count_ == 0) return false;
  for (uint8_t i = 0; i < count_; ++i) if (!bodies_[i].settled) return false;
  return true;
}

}  // namespace dicevessel
