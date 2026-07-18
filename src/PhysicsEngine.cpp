#include "PhysicsEngine.h"

#include <esp_system.h>
#include <math.h>

namespace dicevessel {

static float rnd(float low, float high) {
  return low + (esp_random() / 4294967295.0f) * (high - low);
}

void PhysicsEngine::configure(const RollSpec& spec, int width, int top, int bottom) {
  width_ = width; top_ = top; bottom_ = bottom;
  count_ = min<uint8_t>(spec.diceCount, MAX_BODIES);
  coinMode_ = spec.diceCount == 1 && spec.termCount == 1 && spec.terms[0].sides == 2;
  hidden_ = spec.diceCount > count_ ? spec.diceCount - count_ : 0;
  uint8_t index = 0;
  const float commonRadius = count_ <= 1 ? 24.0f : (count_ <= 4 ? 17.0f : (count_ <= 8 ? 13.0f : 9.0f));
  const uint8_t columns = max<uint8_t>(1, min<uint8_t>(count_, 8));
  const float spacing = width_ / static_cast<float>(columns);
  for (uint8_t t = 0; t < spec.termCount && index < count_; ++t) {
    for (uint8_t n = 0; n < spec.terms[t].count && index < count_; ++n) {
      Body& b = bodies_[index];
      b.sides = spec.terms[t].sides;
      b.radius = b.sides == 2 ? commonRadius * 0.82f : commonRadius;
      const uint8_t col = index % columns;
      const uint8_t row = index / columns;
      b.x = spacing * (col + 0.5f) + rnd(-2, 2);
      b.y = bottom_ - b.radius - row * (commonRadius * 1.55f) + rnd(-2, 2);
      b.vx = b.vy = b.omega = 0;
      b.angle = rnd(-0.5f, 0.5f);
      b.result = 0; b.settled = false; b.revealAt = 0;
      if (coinMode_) { b.x = width_ * 0.5f; b.y = bottom_ - b.radius; }
      ++index;
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
    b.vx += rnd(-105, 105) * e;
    b.vy += rnd(-95, 75) * e;
    b.omega += rnd(-10, 10) * e;
    b.settled = false;
  }
}

void PhysicsEngine::beginDecay(const RollResult& result, uint32_t now) {
  decaying_ = true;
  if (count_ && fabsf(bodies_[0].vx) + fabsf(bodies_[0].vy) < 20) excite(0.9f);
  for (uint8_t i = 0; i < count_; ++i) {
    bodies_[i].result = i < result.count ? result.dice[i].value : 0;
    bodies_[i].settled = false;
    bodies_[i].revealAt = now + (coinMode_ ? 900 : 650 + i * 45 + static_cast<uint32_t>(rnd(0, 180)));
  }
}

void PhysicsEngine::solveWalls(Body& b) {
  const float bounce = 0.72f;
  if (b.x - b.radius < 0) { impact_ = max(impact_, fabsf(b.vx)); b.x = b.radius; b.vx = fabsf(b.vx) * bounce; }
  if (b.x + b.radius >= width_) { impact_ = max(impact_, fabsf(b.vx)); b.x = width_ - b.radius - 1; b.vx = -fabsf(b.vx) * bounce; }
  if (b.y - b.radius < top_) { impact_ = max(impact_, fabsf(b.vy)); b.y = top_ + b.radius; b.vy = fabsf(b.vy) * bounce; }
  if (b.y + b.radius >= bottom_) { impact_ = max(impact_, fabsf(b.vy)); b.y = bottom_ - b.radius - 1; b.vy = -fabsf(b.vy) * bounce; }
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
  const float drag = decaying_ ? powf(0.095f, dt) : powf(0.82f, dt);
  for (uint8_t i = 0; i < count_; ++i) {
    Body& b = bodies_[i];
    if (b.settled) continue;
    if (coinMode_) b.x = width_ * 0.5f;
    b.vy += 28.0f * dt;
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
