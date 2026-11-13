// Adaptive, fallible shooting policies & Pro Primary/Secondary Weapon Switching. GPL-2.0-or-later.
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
  float awpUntil () const {
    return awpUntil_;
  }
};

enum class SwitchReason : uint8_t {
  None = 0,
  AwpPostShotQuickSwitch,       // Pro AWPer fast switch from AWP to Deagle/Secondary after firing
  AwpCloseQuartersDefense,      // Close-range enemy threat while holding AWP -> switch to Deagle/Secondary
  AwpTravelWithDeagle,          // Pro AWPer walking/running to spot with Deagle for light & fast movement (250 u/s)
  RifleEmptyOrLowClipCombat,    // AK-47/M4A1 empty or critically low magazine mid-fight -> switch to Deagle/Secondary
  RifleSprayRecoilFinisher,     // AK-47/M4A1 post-burst/spray tactical switch to Deagle/Secondary
  ReloadInterruptedByEnemy,     // Primary reload interrupted by enemy sight -> cancel reload & draw Secondary
  ReturnToPrimary               // Arrived at spot / engagement cleared / quick-switch complete -> return to Primary (AWP/AK-47/M4A1)
};

struct WeaponSwitchDecision {
  bool shouldSwitch {};
  unsigned targetWeapon {};     // 18=AWP, 28=AK47, 22=M4A1, 26=Deagle, 16=USP, 17=Glock18
  SwitchReason reason { SwitchReason::None };
  bool cancelReload {};
  bool holdingSecondary {};
};

class ProWeaponSwitchController final {
  float switchAt_ { -1.0f };
  float returnAt_ { -1.0f };
  float drawReadyAt_ { -1.0f };
  float lastPrimaryShot_ { -100.0f };
  unsigned lastPrimaryWeapon_ {};
  unsigned activeSecondary_ {};
  SwitchReason activeReason_ { SwitchReason::None };
  bool holdingSecondary_ {};
  uint32_t awpToSecondarySwitches_ {};
  uint32_t rifleToSecondarySwitches_ {};
  uint32_t returnToPrimarySwitches_ {};

public:
  static constexpr unsigned kWeaponUSP     = 16u;
  static constexpr unsigned kWeaponGlock18 = 17u;
  static constexpr unsigned kWeaponAWP     = 18u;
  static constexpr unsigned kWeaponM4A1    = 22u;
  static constexpr unsigned kWeaponDeagle  = 26u;
  static constexpr unsigned kWeaponAK47    = 28u;

  void reset ();
  static constexpr bool isPrimaryWeapon (unsigned w) {
    return w == kWeaponAWP || w == kWeaponAK47 || w == kWeaponM4A1;
  }
  static constexpr bool isSecondaryWeapon (unsigned w) {
    return w == kWeaponDeagle || w == kWeaponUSP || w == kWeaponGlock18;
  }
  static unsigned selectBestSecondary (uint32_t weaponsMask);
  static unsigned selectBestPrimary (uint32_t weaponsMask);

  void onShotFired (
    float now, unsigned w, int remainingPrimaryClip, unsigned firedInBurst,
    bool enemyVisible, float enemyDistance, AimStyle style);

  WeaponSwitchDecision evaluate (
    float now, unsigned currentWeapon, uint32_t weaponsMask,
    int primaryClip, int secondaryClip, bool isReloading,
    bool enemyVisible, float enemyDistance, bool isScoped,
    unsigned firedInBurst, AimStyle style, bool isTravelingToSpot = false);

  bool holdingSecondary () const { return holdingSecondary_; }
  float drawReadyAt () const { return drawReadyAt_; }
  float switchAt () const { return switchAt_; }
  float returnAt () const { return returnAt_; }
  SwitchReason activeReason () const { return activeReason_; }
  uint32_t awpToSecondarySwitches () const { return awpToSecondarySwitches_; }
  uint32_t rifleToSecondarySwitches () const { return rifleToSecondarySwitches_; }
  uint32_t returnToPrimarySwitches () const { return returnToPrimarySwitches_; }
};

struct PenetrationBudget {
  float power {}, range {};
  unsigned walls {};
};
PenetrationBudget penetrationBudget (unsigned weapon);
float materialPower (char material);
bool plausibleWall (unsigned weapon, char material, float thickness, float entryDistance);
} // namespace rbcc
