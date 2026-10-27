// Local online learning for AiBo Human Copycat. GPL-2.0-or-later.
#pragma once
#include "copycat_core.h"
#include <array>

namespace rbcc {
enum class LearnedAction : uint8_t { Native, Hold, Left, Right, Retreat, Count };
enum class TeamTactic : uint8_t { Split, PressureA, PressureB, Count };
constexpr size_t LearningActions = static_cast<size_t> (LearnedAction::Count);
// team * phase * health * visible/range * nearby support * objective * opponent pattern
constexpr size_t LearningStates = 2 * 3 * 2 * 3 * 2 * 2 * 3;
constexpr uint32_t AllLearningActions = (1u << LearningActions) - 1u;
struct LearningCell {
  float recent {}, lifetime {};
  uint32_t visits {}, demonstrations {};
};
struct OpponentPattern {
  float approach {}, longApproach {}, stationary {}, longStationary {};
  uint32_t observations {}, changes {}, lastChange {};
};
struct LearningStats {
  uint32_t updates {}, humanSamples {}, selfPlayUpdates {}, teamRounds {}, rounds {};
};

// Fixed-size tabular Q-learning and team bandits, no external service.
// The engine adapter owns eligibility, visibility, reward attribution and safety.
class OnlineLearning final {
  std::array<std::array<LearningCell, LearningActions>, LearningStates> cells_ {};
  std::array<std::array<LearningCell, 3>, 2> tactics_ {};
  std::array<OpponentPattern, 2> opponents_ {};
  std::array<int, 2> selected_ {{-1, -1}};
  LearningStats stats_ {};
  bool dirty_ {};
public:
  void clear ();
  uint16_t state (const Frame &frame, bool adapt = true) const;
  static LearnedAction classify (const Frame &frame);
  void imitate (uint16_t state, LearnedAction action);
  LearnedAction choose (uint16_t state, uint32_t allowed, uint32_t &random,
    float exploration = 0.06f, bool imitation = true, bool adaptation = true) const;
  void reinforce (uint16_t state, LearnedAction action, float reward,
    uint16_t next, bool terminal, bool selfPlay = false, uint32_t nextAllowed = AllLearningActions);
  void observeOpponent (unsigned team, float approach, bool stationary);
  unsigned pattern (unsigned team) const;
  TeamTactic chooseTactic (unsigned team, uint32_t &random, float exploration = 0.06f);
  void finishTeam (unsigned team, float reward);
  void newRound ();
  const LearningCell &cell (uint16_t state, LearnedAction action) const;
  const LearningCell &tactic (unsigned team, TeamTactic action) const;
  const OpponentPattern &opponent (unsigned team) const;
  const LearningStats &stats () const { return stats_; }
  bool dirty () const { return dirty_; }
  bool save (const std::string &path, const std::string &map, uint32_t environment, std::string &error);
  bool load (const std::string &path, const std::string &map, uint32_t environment, std::string &error);
};
} // namespace rbcc
