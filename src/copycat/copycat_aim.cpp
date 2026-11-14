#if defined(YSTL_TESTS)
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
#else
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
  const bool ak_or_m4 = (w == 28 || w == 22);
  if (distance < 300 || (ak_or_m4 && distance < 550 && level > 0.25f) || (style == AimStyle::Spray && distance < 650 && level > 0.4f))
    return { ShotMode::Spray, 12 + variation % 10, 0.18f, distance < 240 ? 92.0f : 44.0f, distance < 280 ? 12.0f : (ak_or_m4 ? 8.5f : 6.0f) };
  p = { ShotMode::Burst, (ak_or_m4 ? 4u : 2u) + variation % 4, 0.16f, 35, ak_or_m4 ? 5.2f : 3.7f };
  return p;
}
SkillModel expertModel (const SkillModel &base, float level) {
  auto m = base;
  level = unit (level);
  if (level <= 0)
    return m;
  m.turnRate = base.turnRate + (std::max (base.turnRate, 640.0f) - base.turnRate) * level;
  m.reactionMs = base.reactionMs + (std::min (base.reactionMs, 140.0f) - base.reactionMs) * level;
  m.aimError = std::max (0.30f, base.aimError * (1 - 0.72f * level));
  for (unsigned i = 0; i < 31; ++i) {
    m.recoil[i] = base.recoil[i] + (1.0f - base.recoil[i]) * level;
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

void ProWeaponSwitchController::reset () {
  *this = ProWeaponSwitchController {};
}

unsigned ProWeaponSwitchController::selectBestSecondary (uint32_t weaponsMask) {
  if (weaponsMask & (1u << kWeaponDeagle))  return kWeaponDeagle;
  if (weaponsMask & (1u << kWeaponUSP))     return kWeaponUSP;
  if (weaponsMask & (1u << kWeaponGlock18)) return kWeaponGlock18;
  return 0u;
}

unsigned ProWeaponSwitchController::selectBestPrimary (uint32_t weaponsMask) {
  if (weaponsMask & (1u << kWeaponAWP))  return kWeaponAWP;
  if (weaponsMask & (1u << kWeaponAK47)) return kWeaponAK47;
  if (weaponsMask & (1u << kWeaponM4A1)) return kWeaponM4A1;
  return 0u;
}

void ProWeaponSwitchController::onShotFired (
    float now, unsigned w, int remainingPrimaryClip, unsigned firedInBurst,
    bool enemyVisible, float enemyDistance, AimStyle style) {
  if (!std::isfinite (now)) return;
  if (w == kWeaponAWP) {
    lastPrimaryWeapon_ = kWeaponAWP;
    lastPrimaryShot_ = now;
    // Pro AWPer reflex: immediately queue switch to Deagle/Secondary after firing AWP
    switchAt_ = now + (style == AimStyle::Awper ? 0.06f : 0.10f);
    activeReason_ = SwitchReason::AwpPostShotQuickSwitch;
  }
  else if (w == kWeaponAK47 || w == kWeaponM4A1) {
    lastPrimaryWeapon_ = w;
    lastPrimaryShot_ = now;
    if (enemyVisible && enemyDistance < 850.0f) {
      if (remainingPrimaryClip <= 3) {
        switchAt_ = now + 0.05f;
        activeReason_ = SwitchReason::RifleEmptyOrLowClipCombat;
      }
      else if (firedInBurst >= 7u && remainingPrimaryClip <= 8 && enemyDistance < 600.0f) {
        switchAt_ = now + 0.07f;
        activeReason_ = SwitchReason::RifleSprayRecoilFinisher;
      }
    }
  }
}

WeaponSwitchDecision ProWeaponSwitchController::evaluate (
    float now, unsigned currentWeapon, uint32_t weaponsMask,
    int primaryClip, int secondaryClip, bool isReloading,
    bool enemyVisible, float enemyDistance, bool isScoped,
    unsigned firedInBurst, AimStyle style, bool isTravelingToSpot) {
  WeaponSwitchDecision out {};
  if (!std::isfinite (now)) return out;

  const unsigned sec = selectBestSecondary (weaponsMask);
  unsigned pri = selectBestPrimary (weaponsMask);
  if (lastPrimaryWeapon_ != 0u && (weaponsMask & (1u << lastPrimaryWeapon_))) {
    pri = lastPrimaryWeapon_;
  }

  // 1. Currently holding a Primary weapon (AWP, AK-47, or M4A1)
  if (isPrimaryWeapon (currentWeapon)) {
    lastPrimaryWeapon_ = currentWeapon;
    if (sec != 0u && secondaryClip > 0) {
      // 1A. Reloading primary when an enemy appears -> cancel reload and switch to Secondary immediately
      if (isReloading && enemyVisible) {
        switchAt_ = -1.0f;
        holdingSecondary_ = true;
        activeSecondary_ = sec;
        activeReason_ = SwitchReason::ReloadInterruptedByEnemy;
        returnAt_ = now + 1.15f;
        drawReadyAt_ = now + 0.10f;
        if (currentWeapon == kWeaponAWP) ++awpToSecondarySwitches_;
        else ++rifleToSecondarySwitches_;
        out = { true, sec, SwitchReason::ReloadInterruptedByEnemy, true, true };
        return out;
      }

      // 1B. Scheduled post-shot quick-switch ready
      if (switchAt_ >= 0.0f && now >= switchAt_) {
        switchAt_ = -1.0f;
        holdingSecondary_ = true;
        activeSecondary_ = sec;
        if (currentWeapon == kWeaponAWP) {
          const bool fightWithDeagle = (enemyVisible && enemyDistance < 900.0f) || primaryClip <= 0;
          returnAt_ = now + (fightWithDeagle ? 1.25f : (style == AimStyle::Awper ? 0.12f : 0.16f));
          drawReadyAt_ = now + (style == AimStyle::Awper ? 0.08f : 0.11f);
          activeReason_ = SwitchReason::AwpPostShotQuickSwitch;
          ++awpToSecondarySwitches_;
          out = { true, sec, SwitchReason::AwpPostShotQuickSwitch, false, true };
          return out;
        }
        else {
          returnAt_ = now + 1.10f;
          drawReadyAt_ = now + 0.09f;
          if (activeReason_ == SwitchReason::None) activeReason_ = SwitchReason::RifleEmptyOrLowClipCombat;
          ++rifleToSecondarySwitches_;
          out = { true, sec, activeReason_, true, true };
          return out;
        }
      }

      // 1C. Direct AWP close-quarters ambush or dry clip in combat -> switch to Deagle/Secondary
      if (currentWeapon == kWeaponAWP && enemyVisible) {
        if (primaryClip <= 0 || (enemyDistance < 420.0f && (!isScoped || (now - lastPrimaryShot_ < 1.25f)))) {
          switchAt_ = -1.0f;
          holdingSecondary_ = true;
          activeSecondary_ = sec;
          activeReason_ = SwitchReason::AwpCloseQuartersDefense;
          returnAt_ = now + 1.25f;
          drawReadyAt_ = now + 0.09f;
          ++awpToSecondarySwitches_;
          out = { true, sec, SwitchReason::AwpCloseQuartersDefense, true, true };
          return out;
        }
      }

      // 1D. Pro AWP Mobility: while walking/running toward spot without visible enemies, switch AWP -> Deagle for 250 u/s speed!
      if (currentWeapon == kWeaponAWP && !enemyVisible && !isReloading && isTravelingToSpot) {
        switchAt_ = -1.0f;
        returnAt_ = -1.0f;
        holdingSecondary_ = true;
        activeSecondary_ = sec;
        activeReason_ = SwitchReason::AwpTravelWithDeagle;
        drawReadyAt_ = now + 0.08f;
        ++awpToSecondarySwitches_;
        out = { true, sec, SwitchReason::AwpTravelWithDeagle, false, true };
        return out;
      }

      // 1E. Direct AK-47 / M4A1 combat triggers -> switch from Primary rifle to Secondary pistol
      if ((currentWeapon == kWeaponAK47 || currentWeapon == kWeaponM4A1) && enemyVisible) {
        if (primaryClip <= 3 && enemyDistance < 850.0f) {
          switchAt_ = -1.0f;
          holdingSecondary_ = true;
          activeSecondary_ = sec;
          activeReason_ = SwitchReason::RifleEmptyOrLowClipCombat;
          returnAt_ = now + 1.15f;
          drawReadyAt_ = now + 0.09f;
          ++rifleToSecondarySwitches_;
          out = { true, sec, SwitchReason::RifleEmptyOrLowClipCombat, true, true };
          return out;
        }
        if (firedInBurst >= 7u && primaryClip <= 8 && enemyDistance < 600.0f) {
          switchAt_ = -1.0f;
          holdingSecondary_ = true;
          activeSecondary_ = sec;
          activeReason_ = SwitchReason::RifleSprayRecoilFinisher;
          returnAt_ = now + 1.05f;
          drawReadyAt_ = now + 0.09f;
          ++rifleToSecondarySwitches_;
          out = { true, sec, SwitchReason::RifleSprayRecoilFinisher, true, true };
          return out;
        }
      }
    }
  }

  // 2. Currently holding a Secondary weapon (Deagle, USP, or Glock18) or in active secondary switch window
  if (isSecondaryWeapon (currentWeapon) || holdingSecondary_) {
    if (pri != 0u && primaryClip > 0) {
      // Secondary magazine empty while primary has ammo -> immediately switch back to Primary
      if (secondaryClip <= 0) {
        holdingSecondary_ = false;
        returnAt_ = -1.0f;
        switchAt_ = -1.0f;
        activeReason_ = SwitchReason::ReturnToPrimary;
        ++returnToPrimarySwitches_;
        out = { true, pri, SwitchReason::ReturnToPrimary, false, false };
        return out;
      }

      // Handle AWP Travel-With-Deagle state: keep Deagle while moving to spot, switch to AWP upon arrival at spot!
      if (pri == kWeaponAWP && activeReason_ == SwitchReason::AwpTravelWithDeagle) {
        if (enemyVisible) {
          if (enemyDistance < 420.0f) {
            // Close-quarters surprise while running with Deagle -> fight with Deagle!
            activeReason_ = SwitchReason::AwpCloseQuartersDefense;
            returnAt_ = now + 1.25f;
            out.holdingSecondary = true;
            return out;
          }
          // Medium/long-range enemy spotted -> immediately draw AWP!
          holdingSecondary_ = false;
          returnAt_ = -1.0f;
          switchAt_ = -1.0f;
          activeReason_ = SwitchReason::ReturnToPrimary;
          ++returnToPrimarySwitches_;
          out = { true, kWeaponAWP, SwitchReason::ReturnToPrimary, false, false };
          return out;
        }
        if (isTravelingToSpot) {
          // Still running toward spot -> keep holding Deagle for light & fast footsteps (250 u/s)
          holdingSecondary_ = true;
          out.holdingSecondary = true;
          return out;
        }
        // Arrived at spot! Switch from Deagle to AWP immediately!
        holdingSecondary_ = false;
        returnAt_ = -1.0f;
        switchAt_ = -1.0f;
        activeReason_ = SwitchReason::ReturnToPrimary;
        ++returnToPrimarySwitches_;
        out = { true, kWeaponAWP, SwitchReason::ReturnToPrimary, false, false };
        return out;
      }

      // Keep secondary active while fighting a close/mid-range visible enemy if AWP is on cooldown/unsuited or rifle clip is critically low
      if (enemyVisible && secondaryClip > 0) {
        if (pri == kWeaponAWP && (enemyDistance < 600.0f || (now - lastPrimaryShot_ < 1.25f && enemyDistance < 900.0f))) {
          returnAt_ = std::max (returnAt_, now + 0.35f);
        }
        else if ((pri == kWeaponAK47 || pri == kWeaponM4A1) && primaryClip <= 3 && enemyDistance < 800.0f) {
          returnAt_ = std::max (returnAt_, now + 0.45f);
        }
      }

      // If AWP bot finished post-shot quick-switch and is now traveling to reposition with no enemy visible, transition to AwpTravelWithDeagle
      if (pri == kWeaponAWP && !enemyVisible && isTravelingToSpot && returnAt_ >= 0.0f && now >= returnAt_) {
        returnAt_ = -1.0f;
        holdingSecondary_ = true;
        activeReason_ = SwitchReason::AwpTravelWithDeagle;
        out.holdingSecondary = true;
        return out;
      }

      if (returnAt_ >= 0.0f && now >= returnAt_) {
        holdingSecondary_ = false;
        returnAt_ = -1.0f;
        switchAt_ = -1.0f;
        activeReason_ = SwitchReason::ReturnToPrimary;
        ++returnToPrimarySwitches_;
        out = { true, pri, SwitchReason::ReturnToPrimary, false, false };
        return out;
      }
    }
    out.holdingSecondary = holdingSecondary_;
  }
  return out;
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
#endif
