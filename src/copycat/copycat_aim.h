// Adaptive, fallible shooting policies. GPL-2.0-or-later.
#pragma once
#include "copycat_core.h"
#include <map>

namespace rbcc {
struct AimEvidence {
  uint32_t shots {}, hits {}, kills {}, headKills {}, deaths {}, reactions {}, tracking {};
  float reactionMs { 500 }, error { 4 }, stopped { 0.5f }, practiceSeconds {};
  void shot (float errorDegrees, bool stationary);
  void hit ();
  void reaction (float milliseconds);
  void duel (bool won, bool head = false);
  float confidence () const;
  float estimate () const;
};
class AimKnowledge final {
  std::map<uint32_t, AimEvidence> people_ {};
  bool dirty_ {};

public:
  static constexpr unsigned MaxProfiles = 128;
  void clear ();
  AimEvidence *edit (uint32_t profile);
  const AimEvidence *get (uint32_t profile) const;
  bool dirty () const {
    return dirty_;
  }
  size_t size () const {
    return people_.size ();
  }
  bool save (const std::string &path, const std::string &map, uint32_t environment, std::string &error);
  bool load (const std::string &path, const std::string &map, uint32_t environment, std::string &error);
};
struct AdaptiveAimLevel {
  float value {};
  void update (float dt, float evidenceTarget, float confidence);
};
enum class AimStyle : uint8_t {
  Balanced,
  Precision,
  Spray,
  Awper
};
enum class ShotMode : uint8_t {
  Tap,
  Burst,
  Spray,
  Sniper
};
struct ShotPlan {
  ShotMode mode { ShotMode::Tap };
  unsigned bullets { 1 };
  float pause { 0.2f }, maxSpeed { 35 }, recoilLimit { 3 };
};
ShotPlan shootingPlan (unsigned weapon, float distance, float expertise, AimStyle style, uint32_t variation);
SkillModel expertModel (const SkillModel &baseline, float expertise);
class ShotDiscipline final {
  uint32_t target_ {};
  unsigned weapon_ {}, fired_ {};
  float started_ {}, restUntil_ {}, awpUntil_ {};
  ShotPlan plan_ {};
  bool engaged_ {}, rest_ {};

public:
  void reset ();
  void confirmed (float now, unsigned weapon, unsigned bullets);
  bool allow (
    float now, uint32_t target, unsigned weapon, bool request, bool grounded, bool scoped, float speed, float recoil, const ShotPlan &plan);
  const ShotPlan &plan () const {
    return plan_;
  }
  unsigned fired () const {
    return fired_;
  }
};
struct PenetrationBudget {
  float power {}, range {};
  unsigned walls {};
};
PenetrationBudget penetrationBudget (unsigned weapon);
float materialPower (char material);
bool plausibleWall (unsigned weapon, char material, float thickness, float entryDistance);
} // namespace rbcc
