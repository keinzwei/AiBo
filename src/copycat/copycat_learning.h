// Local online learning for AiBo Human Copycat (Level 5 Apex: Self-Modifying Hebbian Plasticity, Structural Neuroevolution, Liquid Recurrent Core, Latent World Model & RND Curiosity). GPL-2.0-or-later.
#pragma once
#include "copycat_core.h"
#include <array>

namespace rbcc {
enum class LearnedAction : uint8_t { Native, Hold, Left, Right, Retreat, Count };
enum class TeamTactic : uint8_t { Split, PressureA, PressureB, Count };
// Level 2–5 High-Level Hierarchical Subgoal Options (multi-step tactical options)
enum class SubgoalOption : uint8_t {
  HoldAngle,
  JiggleBait,
  WideSwing,
  FlankRotate,
  TradeSupport,
  FallBackCover,
  Count
};
constexpr size_t LearningActions = static_cast<size_t> (LearnedAction::Count);
constexpr size_t SubgoalOptionCount = static_cast<size_t> (SubgoalOption::Count);
// team * phase * health * visible/range * nearby support * objective * opponent pattern
constexpr size_t LearningStates = 2 * 3 * 2 * 3 * 2 * 2 * 3;
constexpr uint32_t AllLearningActions = (1u << LearningActions) - 1u;

// Level 1–5 Continuous Perception, Tiny Neural Policy, Latent World Model, RND Curiosity & Liquid/Hebbian dimensions
constexpr size_t ContinuousFeatureDim = 24;
constexpr size_t MlpHiddenDim = 32;
constexpr size_t LatentDim = 16;
constexpr size_t CuriosityEmbedDim = 12;
constexpr size_t LiquidStateDim = 16;
constexpr size_t MaxTrackedPlayers = 32;

struct LearningCell {
  float recent {}, lifetime {};
  uint32_t visits {}, demonstrations {};
};
struct OpponentPattern {
  float approach {}, longApproach {}, stationary {}, longStationary {};
  uint32_t observations {}, changes {}, lastChange {};
};
struct OpponentPlayerProfile {
  uint32_t token {};
  unsigned team {255};
  float aggression {};        // EMA of approach velocity [-1, 1]
  float stationary {};        // EMA of holding/camping ratio [0, 1]
  float strafeRate {};        // EMA of lateral speed [0, 1]
  float crouchRate {};        // EMA of crouch frequency in combat [0, 1]
  float awpPreference {};     // EMA of sniper usage [0, 1]
  float duelWinRate {0.5f};   // EMA of opponent winning duels against bots [0, 1]
  Vec lastPos {};
  uint32_t sightings {}, duels {};
};
struct ContinuousModulator {
  LearnedAction action {LearnedAction::Native};
  SubgoalOption option {SubgoalOption::HoldAngle};
  float strafeScale {0.65f};      // Continuous lateral speed scale [0.40, 1.00]
  float forwardBias {0.0f};       // Forward/retreat diagonal blend [-0.45, 0.45]
  float crouchLikelihood {0.0f};  // Learned micro-crouch peek probability [0.0, 0.65]
  float confidence {0.0f};        // Combined policy confidence [0.0, 1.0]
  float expectedReturn {0.0f};    // Level 4 World Model 3-step imagined return
  float noveltyBonus {0.0f};      // Level 4 RND intrinsic curiosity signal
  float liquidResonance {0.0f};   // Level 5 Liquid Recurrent working-memory activation
  float hebbianModulation {0.0f}; // Level 5 Self-modifying Hebbian synapse drive
};
struct TinyMlpPolicy {
  std::array<std::array<float, ContinuousFeatureDim>, MlpHiddenDim> w1 {};
  std::array<float, MlpHiddenDim> b1 {};
  std::array<std::array<float, MlpHiddenDim>, LearningActions> w2 {};
  std::array<float, LearningActions> b2 {};
  std::array<float, MlpHiddenDim> vw {};
  float vb {};
  std::array<std::array<float, MlpHiddenDim>, 3> mw {};
  std::array<float, 3> mb {};
  uint32_t steps {};
  float avgTdError {};
};
struct HrlOptionPolicy {
  std::array<std::array<float, ContinuousFeatureDim>, SubgoalOptionCount> wOption {};
  std::array<float, SubgoalOptionCount> bOption {};
  std::array<float, SubgoalOptionCount> optionValue {};
  uint32_t switches {}, updates {};
};
// Level 4 Latent World Model (16-D continuous state transition + reward predictor + 3-step MPC imagination)
struct LatentWorldModel {
  std::array<std::array<float, ContinuousFeatureDim>, LatentDim> wEnc {};
  std::array<float, LatentDim> bEnc {};
  std::array<std::array<std::array<float, LatentDim>, LatentDim>, LearningActions> wDyn {};
  std::array<std::array<float, LatentDim>, LearningActions> bDyn {};
  std::array<std::array<float, LatentDim>, LearningActions> wRew {};
  std::array<float, LearningActions> bRew {};
  std::array<float, LatentDim> wVal {};
  float bVal {};
  uint32_t trainSteps {}, rollouts {};
  float avgModelLoss {};
};
struct WorldModelRollout {
  LearnedAction bestAction {LearnedAction::Native};
  float expectedReturn {0.0f};
  float survivalProb {0.5f};
  std::array<float, LearningActions> rolloutScores {};
};
// Level 4 Intrinsic Curiosity Module (Random Network Distillation for open-ended novelty seeking)
struct IntrinsicCuriosityModule {
  std::array<std::array<float, ContinuousFeatureDim>, CuriosityEmbedDim> wTarget {};
  std::array<float, CuriosityEmbedDim> bTarget {};
  std::array<std::array<float, ContinuousFeatureDim>, CuriosityEmbedDim> wPred {};
  std::array<float, CuriosityEmbedDim> bPred {};
  uint32_t steps {};
  float avgCuriosity {};
};
// Level 5 Liquid Time-Constant (LTC) Continuous-Time Recurrent Working Memory
struct LiquidRecurrentCore {
  std::array<float, LiquidStateDim> state {};
  std::array<std::array<float, ContinuousFeatureDim>, LiquidStateDim> wIn {};
  std::array<std::array<float, LiquidStateDim>, LiquidStateDim> wRec {};
  std::array<float, LiquidStateDim> bRec {};
  std::array<float, LiquidStateDim> tauBase {}; // Adaptive time-constants [0.08s, 1.80s]
  std::array<std::array<float, LiquidStateDim>, LearningActions> wReadout {};
  uint32_t ticks {};
};
// Level 5 Self-Modifying ABCD Hebbian Plasticity & Structural Neuroevolution (Dynamic Synaptic Pruning & Neurogenesis)
struct HebbianPlasticCortex {
  // Fast plastic trace H[i][j] + structural topology mask[i][j] over LiquidStateDim -> LearningActions & ContinuousFeatureDim -> LiquidStateDim
  std::array<std::array<float, ContinuousFeatureDim>, LiquidStateDim> plasticTraces {};
  std::array<std::array<float, ContinuousFeatureDim>, LiquidStateDim> synapseUtility {};
  std::array<std::array<uint8_t, ContinuousFeatureDim>, LiquidStateDim> activeMask {};
  // Evolvable ABCD Hebbian local plasticity coefficients per post-synaptic neuron:
  // dW_ij = eta_i * dopamine * (A_i * pre_j * post_i + B_i * pre_j + C_i * post_i + D_i)
  std::array<float, LiquidStateDim> coeffA {};
  std::array<float, LiquidStateDim> coeffB {};
  std::array<float, LiquidStateDim> coeffC {};
  std::array<float, LiquidStateDim> coeffD {};
  std::array<float, LiquidStateDim> eta {};
  uint32_t activeSynapses {};
  uint32_t prunedSynapses {};
  uint32_t sproutedSynapses {};
  uint32_t plasticitySteps {};
  uint32_t topologyRewires {};
};
struct LearningStats {
  uint32_t updates {}, humanSamples {}, selfPlayUpdates {}, teamRounds {}, rounds {};
};

const char *subgoalOptionName (SubgoalOption opt);

// Hybrid Tabular + TinyMLP + Level 2 HRL + Level 4 World Model & RND Curiosity + Level 5 Liquid Recurrent & Self-Modifying Hebbian Cortex.
// 100% local C++20 execution, Single-DLL ready, no external service or GPU required.
class OnlineLearning final {
  std::array<std::array<LearningCell, LearningActions>, LearningStates> cells_ {};
  std::array<std::array<LearningCell, 3>, 2> tactics_ {};
  std::array<OpponentPattern, 2> opponents_ {};
  std::array<OpponentPlayerProfile, MaxTrackedPlayers> players_ {};
  TinyMlpPolicy mlp_ {};
  HrlOptionPolicy hrl_ {};
  LatentWorldModel world_ {};
  IntrinsicCuriosityModule curiosity_ {};
  LiquidRecurrentCore liquid_ {};
  HebbianPlasticCortex hebbian_ {};
  std::array<int, 2> selected_ {{-1, -1}};
  LearningStats stats_ {};
  bool dirty_ {};
  void initMlp ();
  void initHrl ();
  void initWorldModel ();
  void initCuriosity ();
  void initLiquidAndHebbian ();
public:
  OnlineLearning () { initMlp (); initHrl (); initWorldModel (); initCuriosity (); initLiquidAndHebbian (); }
  void clear ();
  uint16_t state (const Frame &frame, bool adapt = true) const;
  std::array<float, ContinuousFeatureDim> extractFeatures (const Frame &frame, bool adapt = true) const;
  std::array<float, ContinuousFeatureDim> featuresFromDiscreteState (uint16_t state) const;
  float evaluateValue (const std::array<float, ContinuousFeatureDim> &features) const;
  std::array<float, LearningActions> evaluatePolicy (const std::array<float, ContinuousFeatureDim> &features,
    ContinuousModulator *mod = nullptr) const;
  // Level 4 Latent World Model & Intrinsic Curiosity API
  std::array<float, LatentDim> encodeLatent (const std::array<float, ContinuousFeatureDim> &features) const;
  std::array<float, LatentDim> predictNextLatent (const std::array<float, LatentDim> &z, LearnedAction action, float *predictedReward = nullptr) const;
  WorldModelRollout imagineRollout (const Frame &frame, uint32_t allowed = AllLearningActions, bool adapt = true) const;
  void trainWorldModel (const std::array<float, ContinuousFeatureDim> &prevFeat, LearnedAction action,
    float actualReward, const std::array<float, ContinuousFeatureDim> &nextFeat, bool terminal);
  float computeCuriosity (const std::array<float, ContinuousFeatureDim> &features) const;
  float observeAndTrainCuriosity (const std::array<float, ContinuousFeatureDim> &features);
  // Level 5 Liquid Recurrent ODE & Self-Modifying Hebbian Plasticity / Structural Neuroevolution API
  std::array<float, LearningActions> stepLiquidAndHebbian (const std::array<float, ContinuousFeatureDim> &features,
    float dt, float neuromodulatorDopamine, float *outResonance = nullptr, float *outHebbianDrive = nullptr);
  void rewireSynapticTopology (float surpriseSignal, uint32_t &rngSeed);
  void applyEvolutionaryDNA (const std::array<float, 5> &abcdEtaGenes, float tauScale);
  static LearnedAction classify (const Frame &frame);
  void imitate (uint16_t state, LearnedAction action);
  void imitateContinuous (const Frame &frame, LearnedAction action, bool adapt = true);
  LearnedAction choose (uint16_t state, uint32_t allowed, uint32_t &random,
    float exploration = 0.06f, bool imitation = true, bool adaptation = true) const;
  SubgoalOption chooseOption (const Frame &frame, SubgoalOption currentOption, float optionElapsedSec,
    uint32_t &random, float exploration = 0.06f, bool adaptation = true);
  bool shouldTerminateOption (const Frame &frame, SubgoalOption currentOption, float optionElapsedSec,
    bool adaptation = true) const;
  ContinuousModulator chooseContinuous (const Frame &frame, uint32_t allowed, uint32_t &random,
    float exploration = 0.06f, bool imitation = true, bool adaptation = true,
    SubgoalOption activeOption = SubgoalOption::WideSwing);
  void reinforce (uint16_t state, LearnedAction action, float reward,
    uint16_t next, bool terminal, bool selfPlay = false, uint32_t nextAllowed = AllLearningActions);
  void reinforceContinuous (const Frame &prevFrame, LearnedAction action, float reward,
    const Frame &nextFrame, bool terminal, bool selfPlay = false, uint32_t nextAllowed = AllLearningActions,
    bool adapt = true, SubgoalOption activeOption = SubgoalOption::WideSwing);
  void observeOpponent (unsigned team, float approach, bool stationary);
  void observePlayer (uint32_t token, unsigned opponentTeam, float approach, float lateralSpeed,
    bool crouched, uint8_t weapon, const Vec &pos);
  void recordDuel (uint32_t token, bool botWon);
  const OpponentPlayerProfile *playerProfile (uint32_t token) const;
  size_t trackedPlayers () const;
  unsigned pattern (unsigned team) const;
  TeamTactic chooseTactic (unsigned team, uint32_t &random, float exploration = 0.06f);
  void finishTeam (unsigned team, float reward);
  void newRound ();
  const LearningCell &cell (uint16_t state, LearnedAction action) const;
  const LearningCell &tactic (unsigned team, TeamTactic action) const;
  const OpponentPattern &opponent (unsigned team) const;
  const TinyMlpPolicy &mlp () const { return mlp_; }
  const HrlOptionPolicy &hrl () const { return hrl_; }
  const LatentWorldModel &worldModel () const { return world_; }
  const IntrinsicCuriosityModule &curiosity () const { return curiosity_; }
  const LiquidRecurrentCore &liquid () const { return liquid_; }
  const HebbianPlasticCortex &hebbian () const { return hebbian_; }
  const LearningStats &stats () const { return stats_; }
  bool dirty () const { return dirty_; }
  bool save (const std::string &path, const std::string &map, uint32_t environment, std::string &error);
  bool load (const std::string &path, const std::string &map, uint32_t environment, std::string &error);
};
} // namespace rbcc
