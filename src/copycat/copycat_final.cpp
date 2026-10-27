// GPL-2.0-or-later. Bounded state, no score/physics manipulation.
#include "copycat_final.h"
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
bool point (const Vec &p) {
  return std::isfinite (p.x) && std::isfinite (p.y) && std::isfinite (p.z) && std::fabs (p.x) < 65536 && std::fabs (p.y) < 65536 &&
         std::fabs (p.z) < 65536;
}
bool sameRegion (const Vec &a, const Vec &b) {
  const auto d = a - b;
  return std::fabs (d.z) < 72 && d.x * d.x + d.y * d.y < 176.0f * 176.0f;
}
void put (std::vector<uint8_t> &b, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    b.push_back (static_cast<uint8_t> (n >> (i * 8u)));
}
void real (std::vector<uint8_t> &b, float f) {
  uint32_t n;
  std::memcpy (&n, &f, 4);
  put (b, n);
}
struct Read {
  const std::vector<uint8_t> &b;
  size_t p {};
  bool ok { true };
  uint32_t get () {
    if (p + 4 > b.size ()) {
      ok = false;
      return 0;
    }
    uint32_t n {};
    for (unsigned i = 0; i < 4; ++i)
      n |= static_cast<uint32_t> (b[p++]) << (i * 8u);
    return n;
  }
  float real () {
    auto n = get ();
    float f;
    std::memcpy (&f, &n, 4);
    return f;
  }
};
bool mapValid (const std::string &m) {
  return !m.empty () && m.size () <= 63 &&
         m.find_first_not_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos;
}
}
void FinalMemory::clear () {
  *this = FinalMemory {};
}
bool FinalMemory::fresh (uint32_t profile, const Vec &foot, unsigned window) const {
  const auto it = histories_.find (profile);
  if (it == histories_.end ())
    return true;
  const auto &h = it->second;
  const unsigned count = std::min ({ window, h.count, 32u });
  for (unsigned i = 0; i < count; ++i)
    if (sameRegion (h.places[(h.cursor + 31u - i) % 32u].foot, foot))
      return false;
  return true;
}
float FinalMemory::bias (uint32_t profile, const Vec &foot) const {
  const auto it = histories_.find (profile);
  if (it == histories_.end ())
    return 0;
  float penalty = 0;
  const auto &h = it->second;
  for (unsigned i = 0; i < std::min (h.count, 32u); ++i) {
    const auto &p = h.places[(h.cursor + 31u - i) % 32u];
    if (sameRegion (p.foot, foot))
      penalty += 650.0f / (1.0f + static_cast<float> (i) * 0.3f);
  }
  return -penalty;
}
void FinalMemory::remember (uint32_t profile, const Vec &foot, PlaceUse use) {
  if (!profile || !point (foot) || static_cast<unsigned> (use) > 4)
    return;
  if (!histories_.count (profile) && histories_.size () >= MaxProfiles)
    return;
  auto &h = histories_[profile];
  if (h.count && sameRegion (h.places[(h.cursor + 31u) % 32u].foot, foot))
    return;
  if (sequence_ < 1000000000u)
    ++sequence_;
  h.places[h.cursor % 32u] = { foot, sequence_, use };
  h.cursor = (h.cursor + 1u) % 32u;
  h.count = std::min (h.count + 1u, 32u);
  dirty_ = true;
}
void FinalMemory::strategy (uint32_t profile, FinalStrategy s) {
  const auto i = static_cast<size_t> (s);
  if (i >= 6 || !profile)
    return;
  if (!histories_.count (profile) && histories_.size () >= MaxProfiles)
    return;
  auto &n = histories_[profile].strategies[i];
  if (n < 1000000000u)
    ++n;
  dirty_ = true;
}
uint32_t FinalMemory::strategies (uint32_t profile, FinalStrategy s) const {
  const auto i = static_cast<size_t> (s);
  const auto it = histories_.find (profile);
  return i < 6 && it != histories_.end () ? it->second.strategies[i] : 0;
}
bool FinalMemory::save (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear ();
  if (!mapValid (map)) {
    error = "invalid map";
    return false;
  }
  std::vector<uint8_t> b { 'H', 'C', 'F', 'I', 'N', 'A', 'L', '1' };
  put (b, env);
  put (b, static_cast<uint32_t> (map.size ()));
  b.insert (b.end (), map.begin (), map.end ());
  put (b, sequence_);
  put (b, static_cast<uint32_t> (histories_.size ()));
  for (const auto &[id, h] : histories_) {
    put (b, id);
    put (b, h.cursor);
    put (b, h.count);
    for (auto n : h.strategies)
      put (b, n);
    for (const auto &p : h.places) {
      real (b, p.foot.x);
      real (b, p.foot.y);
      real (b, p.foot.z);
      put (b, p.sequence);
      put (b, static_cast<uint32_t> (p.use));
    }
  }
  put (b, hashBytes (b.data (), b.size ()));
  const auto tmp = path + ".tmp";
  auto f = std::fopen (tmp.c_str (), "wb");
  if (!f) {
    error = "cannot open final memory temp";
    return false;
  }
  bool ok = std::fwrite (b.data (), 1, b.size (), f) == b.size ();
  if (std::fflush (f))
    ok = false;
  if (std::fclose (f))
    ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA (tmp.c_str (), path.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename (tmp.c_str (), path.c_str ()) == 0;
#endif
  }
  if (!ok) {
    std::remove (tmp.c_str ());
    error = "final memory save failed; old file retained";
    return false;
  }
  dirty_ = false;
  return true;
}
bool FinalMemory::load (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear ();
  auto f = std::fopen (path.c_str (), "rb");
  if (!f) {
    error = "final memory not found";
    return false;
  }
  std::vector<uint8_t> b (256 * 1024);
  const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool io = std::ferror (f) != 0;
  std::fclose (f);
  if (io || size < 28 || size >= b.size () || !mapValid (map)) {
    error = "invalid final memory size";
    return false;
  }
  b.resize (size);
  Read r { b }, tail { b };
  tail.p = size - 4;
  if (std::memcmp (b.data (), "HCFINAL1", 8) || tail.get () != hashBytes (b.data (), size - 4)) {
    error = "final memory header/checksum";
    return false;
  }
  r.p = 8;
  const auto stored = r.get (), len = r.get ();
  if (stored != env || len != map.size () || 16 + len + 12 > size || std::memcmp (b.data () + 16, map.data (), len)) {
    error = "final memory map/physics mismatch";
    return false;
  }
  r.p += len;
  FinalMemory candidate;
  candidate.sequence_ = r.get ();
  const auto count = r.get ();
  if (count > MaxProfiles || candidate.sequence_ > 1000000000u || size != 28 + len + count * (36 + 32 * 20)) {
    error = "invalid final memory count";
    return false;
  }
  for (unsigned i = 0; i < count; ++i) {
    const auto id = r.get ();
    History h;
    h.cursor = r.get ();
    h.count = r.get ();
    if (!id || candidate.histories_.count (id) || h.cursor >= 32 || h.count > 32) {
      error = "invalid final profile";
      return false;
    }
    for (auto &n : h.strategies) {
      n = r.get ();
      if (n > 1000000000u) {
        error = "invalid strategy count";
        return false;
      }
    }
    for (auto &p : h.places) {
      p.foot.x = r.real ();
      p.foot.y = r.real ();
      p.foot.z = r.real ();
      p.sequence = r.get ();
      const auto u = r.get ();
      if (!point (p.foot) || p.sequence > candidate.sequence_ || u > 4) {
        error = "invalid final place";
        return false;
      }
      p.use = static_cast<PlaceUse> (u);
    }
    candidate.histories_.emplace (id, h);
  }
  if (!r.ok || r.p != size - 4) {
    error = "invalid final memory payload";
    return false;
  }
  *this = std::move (candidate);
  dirty_ = false;
  return true;
}
uint32_t FinalPersona::next () {
  if (!random)
    random = 0x9e3779b9u;
  random ^= random << 13u;
  random ^= random >> 17u;
  random ^= random << 5u;
  return random;
}
void FinalPersona::seed (uint32_t s) {
  *this = FinalPersona {};
  random = s ? s : 1;
  ambition = 0.6f + static_cast<float> (next () % 36u) * 0.01f;
  confidence = 0.35f + static_cast<float> (next () % 36u) * 0.01f;
}
void FinalPersona::killed (bool friendly) {
  if (friendly) {
    anger = std::min (anger + 0.15f, 1.0f);
    confidence = std::max (0.0f, confidence - 0.2f);
    return;
  }
  ++streak;
  confidence = std::min (1.0f, confidence + 0.15f);
  fear *= 0.6f;
  anger *= 0.7f;
}
void FinalPersona::died () {
  streak = 0;
  ++deaths;
  confidence = std::max (0.0f, confidence - 0.17f);
  anger = std::min (1.0f, anger + 0.3f);
  fear = std::min (1.0f, fear + 0.2f);
}
void FinalPersona::hurt (float damage) {
  if (!std::isfinite (damage))
    return;
  damage = std::clamp (damage, 0.0f, 100.0f);
  fear = std::min (1.0f, fear + damage * 0.008f);
  anger = std::min (1.0f, anger + damage * 0.003f);
}
void FinalPersona::update (float dt, int health, int score, int leader, int place) {
  if (!std::isfinite (dt))
    return;
  dt = std::clamp (dt, 0.0f, 2.0f);
  anger = std::max (0.0f, anger - dt * 0.012f);
  fear = std::max (0.0f, fear - dt * 0.018f);
  rank = std::max (1, place);
  gap = std::max (0, leader - score);
  top = score > 0 && score >= leader && rank == 1;
  const float urgency = std::min (static_cast<float> (gap) * 0.035f, 0.3f) * ambition;
  const float low = health < 35 ? 0.38f : (health < 60 ? 0.15f : 0.0f);
  risk = std::clamp (0.25f + confidence * 0.5f + anger * 0.25f + urgency - fear * 0.4f - low, 0.08f, 0.95f);
  if (low > 0.3f && fear > 0.35f)
    mood = FinalMood::Afraid;
  else if (anger > 0.55f)
    mood = FinalMood::Angry;
  else if (risk > 0.78f)
    mood = FinalMood::Reckless;
  else if (top || confidence > 0.7f)
    mood = FinalMood::Confident;
  else if (risk < 0.4f)
    mood = FinalMood::Cautious;
  else
    mood = FinalMood::Calm;
}
FinalStrategy FinalPersona::choose () {
  if (!remaining) {
    for (unsigned i = 0; i < 6; ++i)
      bag[i] = static_cast<FinalStrategy> (i);
    for (unsigned i = 5; i > 0; --i)
      std::swap (bag[i], bag[next () % (i + 1)]);
    remaining = 6;
    if (bag[5] == current)
      std::swap (bag[5], bag[0]);
  }
  current = bag[--remaining];
  return current;
}
void PhysicalBoost::start (float now, float rise) {
  phase_ = std::isfinite (rise) && rise >= 0 && rise <= 132 ? BoostPhase::Approach : BoostPhase::Aborted;
  rise_ = rise;
  upperSupport_ = false;
  started_ = phase_start_ = now;
  next_jump_ = now;
  jump_until_ = 0;
}
BoostInput PhysicalBoost::update (float now, const BoostObservation &o) {
  BoostInput input;
  input.phase = phase_;
  if (phase_ == BoostPhase::Done || phase_ == BoostPhase::Aborted)
    return input;
  if (!std::isfinite (now) || o.danger || now < started_ || now - started_ > 10) {
    phase_ = BoostPhase::Aborted;
    input.phase = phase_;
    return input;
  }
  if (o.landed && phase_ != BoostPhase::Approach) {
    phase_ = BoostPhase::Done;
    input.phase = phase_;
    return input;
  }
  if (phase_ == BoostPhase::Approach) {
    if (o.helperReady && o.climberReady) {
      phase_ = BoostPhase::Mount;
      phase_start_ = now;
      next_jump_ = now;
    }
    else if (now - started_ > 4)
      phase_ = BoostPhase::Aborted;
  }
  if (phase_ == BoostPhase::Mount && o.supported) {
    phase_ = BoostPhase::Lift;
    phase_start_ = now;
    next_jump_ = now + 0.12f;
  }
  if (phase_ == BoostPhase::Mount && now - phase_start_ > 4.5f)
    phase_ = BoostPhase::Aborted;
  if (phase_ == BoostPhase::Lift && now - phase_start_ > 5)
    phase_ = BoostPhase::Aborted;
  // High boxes require a genuine second support contact on the standing helper.
  // Do not run off the helper during the first lift and do not fake that contact.
  const bool high = rise_ > 112;
  if (high && phase_ == BoostPhase::Lift && !upperSupport_ && o.helperStanding && o.supported && o.height >= 64) {
    upperSupport_ = true;
    next_jump_ = now + 0.10f;
    jump_until_ = 0;
  }
  input.helperDuck = phase_ == BoostPhase::Approach || phase_ == BoostPhase::Mount || (phase_ == BoostPhase::Lift && o.height < 60);
  if ((phase_ == BoostPhase::Mount || phase_ == BoostPhase::Lift) && now >= next_jump_) {
    jump_until_ = now + 0.07f;
    next_jump_ = now + 0.55f;
  }
  input.jump = now < jump_until_ && (phase_ == BoostPhase::Mount || phase_ == BoostPhase::Lift);
  input.climberDuck = phase_ == BoostPhase::Lift && (high ? (upperSupport_ && !o.supported && o.height > 90) : o.height > 70);
  input.towardLanding = phase_ == BoostPhase::Lift && (high ? (upperSupport_ && o.height > 90) : (o.helperStanding || o.height > 65));
  input.phase = phase_;
  return input;
}
const char *strategyName (FinalStrategy s) {
  static const char *n[] = { "push", "flank", "lurk", "snipe", "explore", "support" };
  return static_cast<unsigned> (s) < 6 ? n[static_cast<unsigned> (s)] : "unknown";
}
const char *moodName (FinalMood m) {
  static const char *n[] = { "calm", "confident", "angry", "afraid", "reckless", "cautious" };
  return static_cast<unsigned> (m) < 6 ? n[static_cast<unsigned> (m)] : "unknown";
}
int finalCapacity (int slots, int humans, int reserved) {
  return std::clamp (slots - std::max (std::max (0, humans), std::max (1, reserved)), 0, 31);
}
} // namespace rbcc
