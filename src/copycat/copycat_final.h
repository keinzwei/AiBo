// Human Copycat FINAL: persistent novelty, competitive personality, physical boost protocol.
// GPL-2.0-or-later.
#pragma once
#include "copycat_core.h"
#include <array>
#include <map>

namespace rbcc {
enum class FinalStrategy : uint8_t {
  Push,
  Flank,
  Lurk,
  Snipe,
  Scout,
  Support,
  Count
};
enum class FinalMood : uint8_t {
  Calm,
  Confident,
  Angry,
  Afraid,
  Reckless,
  Cautious
};
enum class PlaceUse : uint8_t {
  Patrol,
  Camp,
  Hide,
  Snipe,
  Boost
};
struct FinalPlace {
  Vec foot {};
  uint32_t sequence {};
  PlaceUse use { PlaceUse::Patrol };
};
class FinalMemory final {
  struct History {
    std::array<FinalPlace, 32> places {};
    uint32_t cursor {}, count {};
    std::array<uint32_t, 6> strategies {};
  };
  std::map<uint32_t, History> histories_ {};
  uint32_t sequence_ {};
  bool dirty_ {};

public:
  static constexpr size_t MaxProfiles = 128;
  void clear ();
  bool fresh (uint32_t profile, const Vec &foot, unsigned window = 20) const;
  float bias (uint32_t profile, const Vec &foot) const;
  void remember (uint32_t profile, const Vec &foot, PlaceUse use);
  void strategy (uint32_t profile, FinalStrategy strategy);
  uint32_t strategies (uint32_t profile, FinalStrategy strategy) const;
  size_t size () const {
    return histories_.size ();
  }
  uint32_t sequence () const {
    return sequence_;
  }
  bool dirty () const {
    return dirty_;
  }
  bool save (const std::string &path, const std::string &map, uint32_t env, std::string &error);
  bool load (const std::string &path, const std::string &map, uint32_t env, std::string &error);
};
struct FinalPersona {
  uint32_t random { 1 };
  float confidence { 0.5f }, anger {}, fear {}, ambition { 0.7f }, risk { 0.5f };
  int streak {}, deaths {}, rank { 1 }, gap {};
  bool top {};
  FinalMood mood { FinalMood::Calm };
  FinalStrategy current { FinalStrategy::Support };
  std::array<FinalStrategy, 6> bag {};
  unsigned remaining {};
  void seed (uint32_t seed);
  void killed (bool friendly = false);
  void died ();
  void hurt (float damage);
  void update (float dt, int health, int score, int leader, int rank);
  FinalStrategy choose ();
  uint32_t next ();
};
enum class BoostPhase : uint8_t {
  Approach,
  Mount,
  Lift,
  Done,
  Aborted
};
struct BoostObservation {
  bool helperReady {}, climberReady {}, supported {}, helperStanding {}, landed {}, danger {};
  float height {};
};
struct BoostInput {
  bool helperDuck {}, jump {}, climberDuck {}, towardLanding {};
  BoostPhase phase { BoostPhase::Approach };
};
class PhysicalBoost final {
  BoostPhase phase_ { BoostPhase::Approach };
  float started_ {}, phase_start_ {}, next_jump_ {}, jump_until_ {};
  float rise_ { 80 };
  bool upperSupport_ {};

public:
  void start (float now, float landingRise = 80);
  BoostInput update (float now, const BoostObservation &observed);
  BoostPhase phase () const {
    return phase_;
  }
};
const char *strategyName (FinalStrategy strategy);
const char *moodName (FinalMood mood);
int finalCapacity (int maxClients, int humans, int reserved = 1);
} // namespace rbcc
