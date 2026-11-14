#pragma once
#if defined(YSTL_TESTS)
// Human Copycat FINAL: persistent novelty, competitive personality, physical boost protocol.
// GPL-2.0-or-later.
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
#else
// Human Copycat FINAL (Level 5 Apex: Self-Modifying Neuroevolution, Population DNA Pool, ~99.9% Autonomy & 10v10 Pro Tactical AI).
// GPL-2.0-or-later.
#include "copycat_core.h"
#include <array>
#include <map>
#include <string>
#include <vector>

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
// Level 2–5 Asynchronous Team Commander Directives & Cooperative Roles
enum class TeamDirective : uint8_t {
  BalancedSpread,
  SynchronizedCrossfire,
  FakeSplitExecute,
  PinchFlank,
  BaitAndTrade,
  EcoSaveAmbush,
  Count
};
enum class BotTacticalRole : uint8_t {
  EntryFrag,
  TradeSupport,
  CrossfireAnchor,
  FlankLurker,
  SniperOverwatch,
  ObjectiveCarrier,
  Count
};
// Level 4 & 5 Self-Generated Curriculum Goals (autonomous lifelong self-improvement agenda)
enum class CurriculumGoal : uint8_t {
  OpeningEntryDuel,
  CrossfireTradeSync,
  NovelAngleDiscovery,
  SniperCounterBait,
  ClutchSurvivalIsolation,
  SkillEvolutionMastery,
  Count
};
constexpr size_t CurriculumGoalCount = static_cast<size_t> (CurriculumGoal::Count);

struct FinalPlace {
  Vec foot {};
  uint32_t sequence {};
  PlaceUse use { PlaceUse::Patrol };
};

constexpr size_t UtilityFeatureDim = 8;
struct UtilityContext {
  float healthRatio { 1.0f };        // [0, 1]
  float economyRatio { 0.5f };       // [0, 1]
  float numericalAdvantage { 0.0f }; // [-1, 1] (allies - enemies) / 5
  float isSniper { 0.0f };           // 1 if AWP/Scout equipped
  float hasUtility { 0.0f };         // 1 if flash/smoke/HE available
  float roundUrgency { 0.0f };       // [0, 1] bomb planted or low clock
  float opponentAggression { 0.0f }; // [-1, 1] observed opponent push rate
  float personalRisk { 0.5f };       // [0, 1] current persona risk appetite
};

struct SpatialHeatCell {
  Vec center {};
  float threat {};       // Accumulated death/damage danger in this region [0, 10]
  float opportunity {};  // Accumulated kill/objective success in this region [0, 10]
  uint32_t events {};
};

struct TeamWorldSnapshot {
  unsigned team {};
  int aliveAllies { 5 }, aliveEnemies { 5 }, avgMoney { 4000 };
  bool bombPlanted {};
  float roundTimeLeft { 90.0f }, opponentAggression {};
  uint32_t roundNumber { 1 };
  std::vector<int> slots {};
  std::vector<bool> snipers {};
  std::vector<bool> carriers {};
};

struct CommanderOrder {
  TeamDirective directive { TeamDirective::BalancedSpread };
  std::array<BotTacticalRole, 32> roles {};
  std::array<float, 32> crossfireAngles {};
  std::string callout {};
  uint32_t sequence {};
};

class AsyncTacticalCommander final {
  std::array<CommanderOrder, 2> orders_ {};
  std::array<std::array<float, 6>, 2> directiveValues_ {};
  uint32_t dispatches_ {};
public:
  void clear ();
  void dispatch (const TeamWorldSnapshot &snap, const class FinalMemory *mem = nullptr);
  void rewardDirective (unsigned team, float reward);
  const CommanderOrder &order (unsigned team) const;
  uint32_t dispatches () const { return dispatches_; }
};

// Level 4 & 5 Self-Evolving Brain: Self-Generated Curriculum + Progressive Unshackling (~99.9% Autonomy)
struct SelfEvolvingProfile {
  std::array<float, CurriculumGoalCount> mastery { 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f };
  std::array<float, CurriculumGoalCount> progress { 0.15f, 0.15f, 0.15f, 0.15f, 0.15f, 0.15f };
  std::array<uint32_t, CurriculumGoalCount> attempts {};
  CurriculumGoal activeGoal { CurriculumGoal::OpeningEntryDuel };
  float autonomyRatio { 0.999f }; // ~99.9% autonomous control; only ~0.1% raw GoldSrc engine protocol hooks remain
  uint32_t unshackledTicks {}, curriculumCompletions {};
};

// Level 5 Inter-Bot Population Neuroevolution (Darwinian DNA Exchange of Hebbian & Liquid Time-Constant Genes)
constexpr size_t MaxGenomePopulation = 16;
struct BotGenome {
  uint32_t genomeId { 1 };
  uint32_t generation { 1 };
  uint32_t lineageId { 1 };
  // Evolvable ABCD Hebbian plasticity coefficients [A, B, C, D, eta]
  std::array<float, 5> hebbianGenes { 0.42f, 0.06f, -0.05f, 0.0f, 0.085f };
  float liquidTauScale { 1.0f };   // Evolved Liquid Time-Constant working memory speed [0.5, 1.8]
  float motorReflexGain { 1.0f };  // Evolved continuous sensorimotor reflex gain [0.7, 1.4]
  float curiosityDrive { 1.0f };   // Evolved intrinsic novelty-seeking drive [0.5, 1.8]
  float fitness { 0.50f };
  uint32_t episodes {};
};

class NeuroevolutionPool final {
  std::array<BotGenome, MaxGenomePopulation> pop_ {};
  uint32_t generations_ { 1 };
  uint32_t crossovers_ {}, mutations_ {};
  uint32_t nextGenomeId_ { 1 };
public:
  NeuroevolutionPool () { initPopulation (); }
  void initPopulation ();
  void clear () { initPopulation (); }
  const BotGenome &genome (size_t slot) const;
  void reportFitness (size_t slot, float reward);
  uint32_t evolveGeneration (uint32_t &rngSeed);
  const BotGenome &champion () const;
  uint32_t generations () const { return generations_; }
  uint32_t crossovers () const { return crossovers_; }
  uint32_t mutations () const { return mutations_; }
  const std::array<BotGenome, MaxGenomePopulation> &population () const { return pop_; }
  void setPopulation (const std::array<BotGenome, MaxGenomePopulation> &pop, uint32_t gens, uint32_t cross, uint32_t muts, uint32_t nextId) {
    pop_ = pop; generations_ = gens; crossovers_ = cross; mutations_ = muts; nextGenomeId_ = nextId;
  }
};

class SelfEvolvingBrain final {
  std::map<uint32_t, SelfEvolvingProfile> profiles_ {};
  NeuroevolutionPool evoPool_ {};
  uint32_t totalCycles_ {};
public:
  void clear ();
  CurriculumGoal selectCurriculum (uint32_t profile, const UtilityContext &ctx, uint32_t &seed);
  void recordCurriculumOutcome (uint32_t profile, CurriculumGoal goal, float reward);
  float autonomyRatio (uint32_t profile, bool hardEngineRuleActive = false) const;
  bool shouldOverrideLegacyFsm (uint32_t profile, bool hardEngineRuleActive, float *outAutonomy = nullptr);
  const SelfEvolvingProfile &profileState (uint32_t profile) const;
  NeuroevolutionPool &evoPool () { return evoPool_; }
  const NeuroevolutionPool &evoPool () const { return evoPool_; }
  uint32_t totalCycles () const { return totalCycles_; }
  size_t trackedProfiles () const { return profiles_.size (); }
  bool save (const std::string &path, const std::string &map, uint32_t env, std::string &error) const;
  bool load (const std::string &path, const std::string &map, uint32_t env, std::string &error);
};

class FinalMemory final {
  struct History {
    std::array<FinalPlace, 32> places {};
    uint32_t cursor {}, count {};
    std::array<uint32_t, 6> strategies {};
    std::array<float, 6> strategyRewards {};
  };
  std::map<uint32_t, History> histories_ {};
  std::vector<SpatialHeatCell> heatmap_ {};
  uint32_t sequence_ {};
  bool dirty_ {};

public:
  static constexpr size_t MaxProfiles = 128;
  static constexpr size_t MaxHeatmapCells = 256;
  void clear ();
  bool fresh (uint32_t profile, const Vec &foot, unsigned window = 20) const;
  float bias (uint32_t profile, const Vec &foot) const;
  float tacticalBias (uint32_t profile, const Vec &foot, FinalStrategy strategy, float risk = 0.5f) const;
  void remember (uint32_t profile, const Vec &foot, PlaceUse use);
  void strategy (uint32_t profile, FinalStrategy strategy);
  void recordStrategyOutcome (uint32_t profile, FinalStrategy strategy, float reward);
  float strategyValue (uint32_t profile, FinalStrategy strategy) const;
  void recordSpatialEvent (const Vec &foot, float threatDelta, float opportunityDelta);
  void decayHeatmap (float factor = 0.92f);
  float threatAt (const Vec &foot) const;
  float opportunityAt (const Vec &foot) const;
  size_t heatmapSize () const {
    return heatmap_.size ();
  }
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
  std::array<float, 6> strategyValues {};
  std::array<std::array<float, UtilityFeatureDim>, 6> contextWeights {};
  UtilityContext lastContext {};
  uint32_t utilityDecisions {};

  void seed (uint32_t seed);
  void killed (bool friendly = false);
  void died ();
  void hurt (float damage);
  void update (float dt, int health, int score, int leader, int rank);
  FinalStrategy choose ();
  FinalStrategy chooseUtility (const UtilityContext &ctx, const FinalMemory *mem = nullptr, uint32_t profile = 0);
  float evaluateStrategy (FinalStrategy s, const UtilityContext &ctx, const FinalMemory *mem = nullptr, uint32_t profile = 0) const;
  void rewardStrategy (float reward);
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
const char *teamDirectiveName (TeamDirective directive);
const char *tacticalRoleName (BotTacticalRole role);
const char *curriculumGoalName (CurriculumGoal goal);

// Skill 1: Internet Mode Simulator (Simulates online player join/leave activity, 10-30s interval, Max 21 bots)
inline constexpr int kInternetModeMaxBots = 21;
inline constexpr int kInternetModeMinInitialBots = 16;
inline constexpr int kPro10v10TeamSize = 10;
inline constexpr int kPro10v10TotalBots = 20;
inline constexpr float kInternetModeDefaultMinInterval = 10.0f;
inline constexpr float kInternetModeDefaultMaxInterval = 30.0f;

class InternetModeSimulator final {
  float minInterval_ { kInternetModeDefaultMinInterval };
  float maxInterval_ { kInternetModeDefaultMaxInterval };
  int maxBots_ { kInternetModeMaxBots };
  int activeTarget_ { kInternetModeMaxBots };
  float nextChurnTime_ { 0.0f };
  bool initialFillDone_ { false };
  uint32_t joinEvents_ {};
  uint32_t leaveEvents_ {};

public:
  void clear ();
  void configure (float minIntervalSec, float maxIntervalSec, int maxBots);
  float sampleInterval (uint32_t &rngSeed) const;
  int stepTarget (float now, int desiredQuota, int currentBots, int activeHumans, int maxClients, uint32_t &rngSeed);
  void recordJoin (float now, uint32_t &rngSeed);
  void recordLeave (float now, uint32_t &rngSeed);
  int maxBots () const { return maxBots_; }
  float minInterval () const { return minInterval_; }
  float maxInterval () const { return maxInterval_; }
  uint32_t joinEvents () const { return joinEvents_; }
  uint32_t leaveEvents () const { return leaveEvents_; }
  int activeTarget () const { return activeTarget_; }
  bool initialFillDone () const { return initialFillDone_; }
};

// Skill 2: Generative CS 1.6 Nickname Bot Generator (Historical DB + Steam + Non-Steam + Continuous Player Monitoring)
enum class NicknameSource : uint8_t {
  HistoricalDatabase = 0,
  SteamCommunity = 1,
  NonSteamCommunity = 2,
  ContinuousMonitoring = 3,
  Count = 4
};

class GenerativeNicknameEngine final {
  std::vector<std::string> learnedPrefixes_ {};
  std::vector<std::string> learnedMorphemes_ {};
  std::vector<std::string> learnedSuffixes_ {};
  std::vector<std::string> observedPlayerNames_ {};
  std::array<std::array<uint16_t, 38>, 38> charMarkov_ {};
  std::array<uint32_t, 256> recentHashes_ {};
  uint32_t recentCursor_ {};
  uint32_t generatedCount_ {};
  uint32_t monitoredPlayers_ {};
  uint32_t sourceCursor_ {};
  bool initialized_ { false };
  bool dirty_ { false };

  void ensureInitialized ();
  void trainMarkovOnStem (const std::string &stem);
  std::string sampleMarkovSyllable (uint32_t &seed, size_t minLen, size_t maxLen) const;
  std::string synthesizeHistorical (uint32_t &seed) const;
  std::string synthesizeSteamCommunity (uint32_t &seed) const;
  std::string synthesizeNonSteamCommunity (uint32_t &seed) const;
  std::string synthesizeMonitored (uint32_t &seed) const;

public:
  GenerativeNicknameEngine () { ensureInitialized (); }
  void clear ();
  void observePlayer (const std::string &playerName);
  std::string generateUnique (uint32_t &seed, const std::vector<std::string> &activeNames = {}, NicknameSource *outSource = nullptr);
  bool save (const std::string &path, std::string &error);
  bool load (const std::string &path, std::string &error);
  uint32_t generatedCount () const { return generatedCount_; }
  uint32_t monitoredPlayers () const { return monitoredPlayers_; }
  size_t learnedMorphemeCount () const { return learnedMorphemes_.size (); }
  bool dirty () const { return dirty_; }
};

const char *nicknameSourceName (NicknameSource src);

// Internet Mode Capacity: Max 21 bots (auto-vacates when server slots are full)
int finalCapacity (int maxClients, int humans, int reserved = 1);

// ============================================================================
// ADVANCED TACTICAL BOT AI (CS 1.6 DE_DUST2 — 10 vs 10 / 20 Bots Pro Engine)
// ============================================================================

// 1. 10 Advanced Counter-Terrorist (CT) Strategies (Offensive + Defensive Mix)
enum class CTAdvancedStrategy : uint8_t {
  AggressiveALongPush = 0,           // 1. Aggressive A Long Push
  AggressiveBTunnelsPush = 1,        // 2. Aggressive B Tunnels Push
  AggressiveMidControl = 2,          // 3. Aggressive Mid Control
  ShortCatwalkRush = 3,              // 4. Short / Catwalk Rush
  LowerTunnelsFlanking = 4,          // 5. Lower Tunnels Flanking
  TRampTSpawnInfiltration = 5,       // 6. T Ramp / T Spawn Infiltration
  TacticalBaitFakePressure = 6,      // 7. Tactical Bait / Fake Pressure
  CoordinatedCounterAttackRetake = 7,// 8. Coordinated Counter-Attack / Bombsite Retake
  DefensiveCrossfire = 8,            // 9. Defensive Crossfire
  DynamicRotationReinforcement = 9,  // 10. Dynamic Rotation and Reinforcement
  Count = 10
};

// 2. 10 Terrorist (T) 10-Player Offensive Strategies
enum class TOffensiveStrategy : uint8_t {
  ALongAttack = 0,          // 1. A Long attacks
  MidControl = 1,           // 2. Mid control
  AShortAttack = 2,         // 3. A Short attacks
  BTunnelsAttack = 3,       // 4. B Tunnels attacks
  SplitAttack = 4,          // 5. Split attacks (A Split / Mid-to-B Split)
  FakeAttack = 5,           // 6. Fake attacks
  TacticalRotation = 6,     // 7. Tactical rotations
  AWPSupport = 7,           // 8. AWP support
  BombCarrierProtection = 8,// 9. Bomb carrier protection
  PostPlantDefense = 9,     // 10. Post-plant defense
  Count = 10
};

// 3. 10 Professional Player Roles (10 vs 10 Role Distribution)
enum class ProRole10v10 : uint8_t {
  AWPSniper = 0,          // 1. AWP Sniper
  EntryFragger = 1,       // 2. Entry Fragger
  SupportPlayer = 2,      // 3. Support Player
  SiteAnchor = 3,         // 4. Site Anchor
  RifleDefender = 4,      // 5. Rifle Defender
  Lurker = 5,             // 6. Lurker
  Flanker = 6,            // 7. Flanker
  TacticalPlaymaker = 7,  // 8. Tactical Playmaker
  MidController = 8,      // 9. Mid Controller
  RotationalDefender = 9, // 10. Rotational Defender
  Count = 10
};

// 4. Dynamic 10v10 Team Formations (A Site / Mid / B Site)
enum class DynamicFormation10v10 : uint8_t {
  Formation_4A_2Mid_4B = 0, // 4 A Site / 2 Mid / 4 B Site
  Formation_3A_4Mid_3B = 1, // 3 A Site / 4 Mid / 3 B Site
  Formation_5A_2Mid_3B = 2, // 5 A Site / 2 Mid / 3 B Site
  Formation_3A_2Mid_5B = 3, // 3 A Site / 2 Mid / 5 B Site
  Count = 4
};

// 5. 8-State Dynamic Tactical Decision Engine
enum class TacticalDecision8 : uint8_t {
  ATTACK = 0,
  DEFEND = 1,
  PUSH = 2,
  FLANK = 3,
  ROTATE = 4,
  RETREAT = 5,
  HOLD = 6,
  RETAKE = 7,
  Count = 8
};

// 6. AWP Position Intelligence on de_dust2 (19 T AWP Spots + 21 CT AWP Spots)
enum class TAwpSpotId : uint8_t {
  TSpawn = 0,
  Suicide = 1,
  TopMid = 2,
  Xbox = 3,
  MidToB = 4,
  TMidDoors = 5,
  LongDoors = 6,
  LongCorner = 7,
  OutsideLong = 8,
  ALong = 9,
  Pit = 10,
  PitCorner = 11,
  AShort = 12,
  ShortCorner = 13,
  UpperBTunnels = 14,
  LowerBTunnels = 15,
  BTunnelExit = 16,
  BDoors = 17,
  BTunnelStairs = 18,
  Count = 19
};

enum class CTAwpSpotId : uint8_t {
  CTSpawn = 0,
  CTMid = 1,
  MidDoors = 2,
  BWindow = 3,
  BSite = 4,
  BPlatform = 5,
  BackPlatform = 6,
  BackSite = 7,
  BCar = 8,
  BDoors = 9,
  BDoubleBox = 10,
  ASite = 11,
  ARamp = 12,
  Goose = 13,
  ASiteBoxes = 14,
  ALongCorner = 15,
  Blue = 16,
  Pit = 17,
  AShortCatwalk = 18,
  ShortStairs = 19,
  ASiteCorner = 20,
  Count = 21
};

struct Dust2AwpSpot {
  const char *name {};
  Vec origin {};
  Vec watchAngles {};  // pitch, yaw, roll
  Vec retreatOrigin {};
  uint8_t lane {};     // 0 = A Site / Long / Short, 1 = B Site / Tunnels, 2 = Mid / Spawn
  bool aggressive {};
};

struct TacticalBotInput {
  int slot { -1 };
  unsigned team { 1 }; // 0 = Terrorist, 1 = CT
  Vec origin {};
  int health { 100 };
  int armor { 100 };
  int money { 4000 };
  bool hasAwp {};
  bool hasC4 {};
  bool hasDefuser {};
  bool hasFlash {};
  bool hasSmoke {};
  bool hasHe {};
  bool seeingEnemy {};
  bool hearingEnemy {};
  int visibleEnemies {};
  int nearbyAllies {};
  int aliveAllies { 10 };
  int aliveEnemies { 10 };
  bool bombPlanted {};
  bool c4SpottedOrHeard {};
  Vec knownBombOrigin {};
  float roundTimeLeft { 105.0f };
  float elapsedRoundTime { 10.0f };
};

struct TacticalBotDirective {
  ProRole10v10 role { ProRole10v10::RifleDefender };
  TacticalDecision8 decision { TacticalDecision8::DEFEND };
  CTAdvancedStrategy ctStrategy { CTAdvancedStrategy::DefensiveCrossfire };
  TOffensiveStrategy tStrategy { TOffensiveStrategy::SplitAttack };
  DynamicFormation10v10 formation { DynamicFormation10v10::Formation_4A_2Mid_4B };
  uint8_t assignedLane { 0 }; // 0 = A Site, 1 = B Site, 2 = Mid
  Vec targetOrigin {};
  Vec watchAngles {};
  Vec retreatOrigin {};
  int awpSpotIndex { -1 };
  const char *awpSpotName { "" };
  bool shouldHoldAngle {};
  bool shouldThrowFlash {};
  bool shouldThrowSmoke {};
  bool shouldRepositionAwp {};
};

class ProTactical10v10Engine final {
  CTAdvancedStrategy activeCtStrategy_ { CTAdvancedStrategy::AggressiveALongPush };
  TOffensiveStrategy activeTStrategy_ { TOffensiveStrategy::SplitAttack };
  DynamicFormation10v10 activeFormation_ { DynamicFormation10v10::Formation_4A_2Mid_4B };
  std::array<ProRole10v10, 32> slotRoles_ {};
  std::array<uint8_t, 32> slotLanes_ {}; // 0 = A, 1 = B, 2 = Mid
  std::array<TacticalDecision8, 32> slotDecisions_ {};
  std::array<int, 32> slotAwpSpots_ {};
  std::array<uint32_t, 32> slotAwpShots_ {};
  std::array<float, 32> slotLastReposition_ {};
  std::array<float, 10> ctStrategyScores_ {};
  std::array<float, 10> tStrategyScores_ {};
  // Fair Shared Enemy Information (Strictly from visual sight or audible sound — NO wallhack!)
  bool sharedEnemyValid_ { false };
  Vec sharedEnemyOrigin_ {};
  float sharedEnemyTimestamp_ { -100.0f };
  uint8_t sharedEnemySector_ { 2 }; // 0 = A, 1 = B, 2 = Mid
  bool sharedC4Alert_ { false };
  Vec sharedC4Origin_ {};
  uint8_t sharedC4Site_ { 0 }; // 0 = A, 1 = B
  float sharedC4Timestamp_ { -100.0f };
  // Adaptive Enemy Pattern Analysis across rounds
  std::array<uint32_t, 3> observedEnemyHits_ { 1, 1, 1 }; // [0]=A, [1]=B, [2]=Mid
  float observedEnemyAggression_ { 0.5f };
  uint32_t roundNumber_ { 0 };

public:
  ProTactical10v10Engine () { clear (); }
  void clear ();
  void startNewRound (uint32_t roundNum, uint32_t &rngSeed, float ctAggressionBias = 0.5f);
  void assignTeamRoster (unsigned team, const std::vector<int> &slots, const std::vector<bool> &hasAwp, const std::vector<bool> &hasC4, uint32_t &rngSeed);
  // Fair info sharing: only call when a bot legitimately sees or hears an enemy / C4
  void reportLegitimateContact (unsigned reportingTeam, const Vec &enemyOrigin, bool hasC4OrPlanted, float now);
  void notifyAwpShotFired (int slot, float now, uint32_t &rngSeed);
  void recordRoundResult (unsigned winningTeam, uint8_t attackedSite);
  TacticalBotDirective evaluateBot (const TacticalBotInput &in, float now, uint32_t &rngSeed);

  CTAdvancedStrategy activeCtStrategy () const { return activeCtStrategy_; }
  TOffensiveStrategy activeTStrategy () const { return activeTStrategy_; }
  DynamicFormation10v10 activeFormation () const { return activeFormation_; }
  ProRole10v10 roleForSlot (int slot) const { return (slot >= 0 && slot < 32) ? slotRoles_[static_cast<size_t> (slot)] : ProRole10v10::RifleDefender; }
  uint8_t laneForSlot (int slot) const { return (slot >= 0 && slot < 32) ? slotLanes_[static_cast<size_t> (slot)] : 0u; }
  TacticalDecision8 decisionForSlot (int slot) const { return (slot >= 0 && slot < 32) ? slotDecisions_[static_cast<size_t> (slot)] : TacticalDecision8::DEFEND; }
  int awpSpotForSlot (int slot) const { return (slot >= 0 && slot < 32) ? slotAwpSpots_[static_cast<size_t> (slot)] : -1; }
  void getFormationCounts (DynamicFormation10v10 fmt, int &outA, int &outMid, int &outB) const;

  static constexpr int kDefaultAwpCompositionPercent = 50;
  static constexpr int kDefaultRifleCompositionPercent = 50;
  static int computeAwpQuota (int teamSize, int awpPercent = kDefaultAwpCompositionPercent, unsigned roundNum = 1u);
  static void buildBalancedTeamLoadout (
    unsigned team, int teamSize, unsigned roundNum,
    std::vector<uint8_t> &outPrimaryWeapons,
    std::vector<uint8_t> &outSecondaryWeapons,
    std::vector<bool> &outHasAwp);

  static const std::array<Dust2AwpSpot, static_cast<size_t> (TAwpSpotId::Count)> &tAwpSpots ();
  static const std::array<Dust2AwpSpot, static_cast<size_t> (CTAwpSpotId::Count)> &ctAwpSpots ();
};

const char *ctStrategyName (CTAdvancedStrategy s);
const char *tStrategyName (TOffensiveStrategy s);
const char *proRole10v10Name (ProRole10v10 r);
const char *dynamicFormationName (DynamicFormation10v10 f);
const char *tacticalDecisionName (TacticalDecision8 d);
ProTactical10v10Engine &proTacticalEngine ();

} // namespace rbcc
#endif
