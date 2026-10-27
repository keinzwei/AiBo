// GPL-2.0-or-later. No spread, damage, position, velocity or weapon timer writes.
#include "copycat_aim.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif
namespace rbcc {
namespace {
float bounded (float v, float lo, float hi) {
  return std::isfinite (v) ? std::clamp (v, lo, hi) : lo;
}
float unit (float v) {
  return bounded (v, 0, 1);
}
bool validMap (const std::string &s) {
  return !s.empty () && s.size () <= 63 &&
         s.find_first_not_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos;
}
void put (std::vector<uint8_t> &b, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    b.push_back (static_cast<uint8_t> (n >> (8 * i)));
}
void real (std::vector<uint8_t> &b, float v) {
  uint32_t n;
  std::memcpy (&n, &v, 4);
  put (b, n);
}
uint32_t read32 (const std::vector<uint8_t> &b, size_t &p) {
  uint32_t n = 0;
  for (unsigned i = 0; i < 4; ++i)
    n |= static_cast<uint32_t> (b[p++]) << (8 * i);
  return n;
}
float number (const std::vector<uint8_t> &b, size_t &p) {
  const auto n = read32 (b, p);
  float v;
  std::memcpy (&v, &n, 4);
  return v;
}
}
void AimEvidence::shot (float degrees, bool stationary) {
  if (!std::isfinite (degrees))
    return;
  if (shots >= 600) {
    shots /= 2;
    hits /= 2;
    kills /= 2;
    headKills /= 2;
    deaths /= 2;
  }
  ++shots;
  ++tracking;
  const float gain = tracking < 12 ? 1.0f / static_cast<float> (tracking) : 0.08f;
  error += (bounded (degrees, 0, 30) - error) * gain;
  stopped += ((stationary ? 1.0f : 0.0f) - stopped) * gain;
  tracking = std::min (tracking, 1000000u);
}
void AimEvidence::hit () {
  if (hits < shots)
    ++hits;
}
void AimEvidence::reaction (float ms) {
  if (!std::isfinite (ms) || ms < 100 || ms > 1500)
    return;
  ++reactions;
  reactionMs += (ms - reactionMs) * (reactions < 10 ? 1.0f / static_cast<float> (reactions) : 0.1f);
  reactions = std::min (reactions, 1000000u);
}
void AimEvidence::duel (bool won, bool head) {
  if (won) {
    kills = std::min (kills + 1u, 1000000u);
    if (head)
      headKills = std::min (headKills + 1u, kills);
  }
  else
    deaths = std::min (deaths + 1u, 1000000u);
}
float AimEvidence::confidence () const {
  const auto encounters = std::max (reactions, kills + deaths);
  if (shots < 20 || encounters < 2)
    return 0;
  return std::min (static_cast<float> (shots) / 80.0f, 1.0f) * std::min (static_cast<float> (encounters) / 6.0f, 1.0f);
}
float AimEvidence::estimate () const {
  const float accuracy = (static_cast<float> (hits) + 2) / (static_cast<float> (shots) + 10);
  const float heads = (static_cast<float> (headKills) + 1) / (static_cast<float> (kills) + 5);
  float quality = 0.40f * unit ((accuracy - 0.16f) / 0.43f) + 0.15f * unit ((heads - 0.12f) / 0.5f) + 0.20f * unit ((650 - reactionMs) / 460) +
                  0.15f * unit ((5.0f - error) / 4.5f) + 0.10f * unit ((stopped - 0.2f) / 0.8f);
  if (kills + deaths >= 6)
    quality = quality * 0.85f + unit ((static_cast<float> (kills) + 1) / (static_cast<float> (kills + deaths) + 2)) * 0.15f;
  return unit (quality);
}
void AimKnowledge::clear () {
  people_.clear ();
  dirty_ = false;
}
AimEvidence *AimKnowledge::edit (uint32_t id) {
  if (!id || (!people_.count (id) && people_.size () >= MaxProfiles))
    return nullptr;
  dirty_ = true;
  return &people_[id];
}
const AimEvidence *AimKnowledge::get (uint32_t id) const {
  const auto i = people_.find (id);
  return i == people_.end () ? nullptr : &i->second;
}
bool AimKnowledge::save (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear ();
  if (!validMap (map)) {
    error = "invalid aim map";
    return false;
  }
  std::vector<uint8_t> b { 'H', 'C', 'A', 'I', 'M', '0', '0', '1' };
  put (b, env);
  put (b, static_cast<uint32_t> (map.size ()));
  b.insert (b.end (), map.begin (), map.end ());
  put (b, static_cast<uint32_t> (people_.size ()));
  for (const auto &[id, s] : people_) {
    put (b, id);
    for (auto n : { s.shots, s.hits, s.kills, s.headKills, s.deaths, s.reactions, s.tracking })
      put (b, n);
    real (b, s.reactionMs);
    real (b, s.error);
    real (b, s.stopped);
    real (b, s.practiceSeconds);
  }
  put (b, hashBytes (b.data (), b.size ()));
  const auto temp = path + ".tmp";
  auto f = std::fopen (temp.c_str (), "wb");
  if (!f) {
    error = "cannot open aim temp";
    return false;
  }
  bool ok = std::fwrite (b.data (), 1, b.size (), f) == b.size ();
  if (std::fflush (f))
    ok = false;
  if (std::fclose (f))
    ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA (temp.c_str (), path.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename (temp.c_str (), path.c_str ()) == 0;
#endif
  }
  if (!ok) {
    std::remove (temp.c_str ());
    error = "aim save failed; previous file retained";
    return false;
  }
  dirty_ = false;
  return true;
}
bool AimKnowledge::load (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear ();
  auto f = std::fopen (path.c_str (), "rb");
  if (!f) {
    error = "aim data not found";
    return false;
  }
  std::vector<uint8_t> b (8192);
  const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool io = std::ferror (f) != 0;
  std::fclose (f);
  auto fail = [&] () {
    error = "invalid aim data; active memory retained";
    return false;
  };
  if (io || size < 24 || size >= b.size () || !validMap (map))
    return fail ();
  b.resize (size);
  size_t tail = size - 4;
  if (std::memcmp (b.data (), "HCAIM001", 8) || read32 (b, tail) != hashBytes (b.data (), size - 4))
    return fail ();
  size_t pos = 8;
  const auto stored = read32 (b, pos), len = read32 (b, pos);
  if (stored != env || len != map.size () || 24 + len > size || std::memcmp (b.data () + pos, map.data (), len))
    return fail ();
  pos += len;
  const auto count = read32 (b, pos);
  if (count > MaxProfiles || size != 24 + len + count * 48)
    return fail ();
  AimKnowledge next;
  for (unsigned i = 0; i < count; ++i) {
    const auto id = read32 (b, pos);
    if (!id || next.people_.count (id))
      return fail ();
    AimEvidence s;
    s.shots = read32 (b, pos);
    s.hits = read32 (b, pos);
    s.kills = read32 (b, pos);
    s.headKills = read32 (b, pos);
    s.deaths = read32 (b, pos);
    s.reactions = read32 (b, pos);
    s.tracking = read32 (b, pos);
    s.reactionMs = number (b, pos);
    s.error = number (b, pos);
    s.stopped = number (b, pos);
    s.practiceSeconds = number (b, pos);
    if (s.shots > 1000000 || s.hits > s.shots || s.kills > 1000000 || s.headKills > s.kills || s.deaths > 1000000 || s.reactions > 1000000 ||
        s.tracking > 1000000 || !std::isfinite (s.reactionMs) || s.reactionMs < 100 || s.reactionMs > 1500 || !std::isfinite (s.error) ||
        s.error < 0 || s.error > 30 || !std::isfinite (s.stopped) || s.stopped < 0 || s.stopped > 1 || !std::isfinite (s.practiceSeconds) ||
        s.practiceSeconds < 0 || s.practiceSeconds > 10000000)
      return fail ();
    next.people_[id] = s;
  }
  people_.swap (next.people_);
  dirty_ = false;
  return true;
}
void AdaptiveAimLevel::update (float dt, float target, float certainty) {
  if (!std::isfinite (dt) || !std::isfinite (target) || !std::isfinite (certainty) || dt <= 0)
    return;
  dt = std::min (dt, 2.0f);
  target = unit (target) * unit (certainty);
  if (std::fabs (target - value) < 0.04f)
    return;
  const float step = (target > value ? 0.02f : 0.008f) * dt;
  value = unit (value + std::clamp (target - value, -step, step));
}
ShotPlan shootingPlan (unsigned w, float distance, float level, AimStyle style, uint32_t variation) {
  ShotPlan p;
  level = unit (level);
  const bool pistol = w == 16 || w == 17 || w == 26;
  if (w == 18)
    return { ShotMode::Sniper, 1, 1.45f, 10, 1.5f };
  if (pistol)
    return { ShotMode::Tap, 1, w == 26 ? 0.23f : 0.16f, distance > 700 ? 30.0f : 65.0f, 2.5f };
  if (distance > 850) {
    if (level > 0.8f && style == AimStyle::Precision && variation % 3 == 0)
      return { ShotMode::Burst, 4 + (variation / 3) % 3, 0.32f, 22, 2.1f };
    return { ShotMode::Tap, 1, 0.23f, 26, 1.6f };
  }
  if (distance < 300 || (style == AimStyle::Spray && distance < 600 && level > 0.5f))
    return { ShotMode::Spray, 12 + variation % 9, 0.20f, distance < 220 ? 90.0f : 38.0f, distance < 220 ? 10.0f : 6.0f };
  p = { ShotMode::Burst, 2 + variation % 4, 0.16f, 35, 3.7f };
  return p;
}
SkillModel expertModel (const SkillModel &base, float level) {
  auto m = base;
  level = unit (level);
  if (level <= 0)
    return m;
  m.turnRate = base.turnRate + (std::max (base.turnRate, 520.0f) - base.turnRate) * level;
  m.reactionMs = base.reactionMs + (std::min (base.reactionMs, 155.0f) - base.reactionMs) * level;
  m.aimError = std::max (0.10f, base.aimError * (1 - 0.82f * level));
  for (unsigned i = 0; i < 31; ++i) {
    m.recoil[i] = base.recoil[i] + (0.98f - base.recoil[i]) * level;
    m.recoilSamples[i] = std::max (m.recoilSamples[i], 1u);
  }
  return m;
}
void ShotDiscipline::reset () {
  *this = ShotDiscipline {};
}
void ShotDiscipline::confirmed (float now, unsigned w, unsigned bullets) {
  if (!std::isfinite (now) || !bullets)
    return;
  if (w == 18)
    awpUntil_ = std::max (awpUntil_, now + 1.45f);
  if (engaged_ && w == weapon_)
    fired_ = std::min (fired_ + std::min (bullets, 6u), 100u);
}
bool ShotDiscipline::allow (
  float now, uint32_t target, unsigned w, bool request, bool ground, bool scoped, float speed, float recoil, const ShotPlan &plan) {
  if (!std::isfinite (now) || !std::isfinite (speed) || !std::isfinite (recoil))
    return false;
  if (w != weapon_ || target != target_) {
    weapon_ = w;
    target_ = target;
    fired_ = 0;
    engaged_ = false;
    rest_ = false;
    restUntil_ = 0;
  }
  if (!request || !target) {
    if (!target)
      engaged_ = false;
    return false;
  }
  if (now < restUntil_ || (w == 18 && now < awpUntil_))
    return false;
  if (!ground || speed > (engaged_ ? plan_.maxSpeed : plan.maxSpeed) || (w == 18 && !scoped))
    return false;
  if (rest_) {
    rest_ = false;
    engaged_ = false;
    fired_ = 0;
  }
  if (!engaged_) {
    plan_ = plan;
    fired_ = 0;
    started_ = now;
    engaged_ = true;
  }
  const float timeout = plan_.mode == ShotMode::Spray ? 1.8f : (plan_.mode == ShotMode::Burst ? 0.8f : 0.3f);
  if (fired_ >= plan_.bullets || now - started_ > timeout || (fired_ > 0 && recoil > plan_.recoilLimit)) {
    rest_ = true;
    restUntil_ = now + plan_.pause;
    return false;
  }
  return true;
}
PenetrationBudget penetrationBudget (unsigned w) {
  switch (w) {
  case 28:
    return { 39, 5000, 1 };
  case 22:
  case 15:
    return { 35, 4000, 1 };
  case 18:
    return { 45, 8000, 2 };
  case 26:
    return { 30, 1000, 1 };
  default:
    return {};
  }
}
float materialPower (char m) {
  switch (m) {
  case 'M':
    return 0.15f;
  case 'C':
    return 0.25f;
  case 'G':
  case 'V':
    return 0.5f;
  case 'T':
    return 0.65f;
  case 'P':
    return 0.4f;
  default:
    return 1;
  }
}
bool plausibleWall (unsigned w, char material, float thickness, float distance) {
  const auto p = penetrationBudget (w);
  return std::isfinite (thickness) && std::isfinite (distance) && thickness > 0 && distance >= 0 && distance < p.range && p.walls > 0 &&
         thickness <= p.power * materialPower (material);
}
} // namespace rbcc
