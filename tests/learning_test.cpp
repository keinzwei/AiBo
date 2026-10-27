// GPL-2.0-or-later. Algorithm and persistence regression tests.
#include "../src/copycat/copycat_learning.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit (1); } } while (false)
using namespace rbcc;
static std::vector<uint8_t> read (const char *path) {
  std::ifstream f (path, std::ios::binary); return {std::istreambuf_iterator<char> (f), {}};
}
static void write (const char *path, const std::vector<uint8_t> &b) {
  std::ofstream f (path, std::ios::binary | std::ios::trunc); f.write (reinterpret_cast<const char *> (b.data ()), static_cast<std::streamsize> (b.size ()));
}
int main () {
  OnlineLearning l; Frame f; f.context.team = 0; f.context.visible = 1; f.context.enemyDistance = 600;
  const auto state = l.state (f); uint32_t seed = 7;
  CHECK (l.choose (state, AllLearningActions, seed, 0) == LearnedAction::Native);
  for (int i = 0; i < 100; ++i) {
    l.reinforce (state, LearnedAction::Left, 1, state, true, true);
    l.reinforce (state, LearnedAction::Right, -1, state, true);
  }
  CHECK (l.choose (state, AllLearningActions, seed, 0, false) == LearnedAction::Left);
  CHECK (l.stats ().selfPlayUpdates == 100 && l.stats ().updates == 200);
  CHECK (l.cell (state, LearnedAction::Left).recent > 0.99f);
  // A new opponent punishes an old successful action. Recency adapts without deleting lifetime evidence.
  for (int i = 0; i < 80; ++i) l.reinforce (state, LearnedAction::Left, -1, state, true);
  CHECK (l.choose (state, AllLearningActions, seed, 0, false) == LearnedAction::Native);
  CHECK (l.cell (state, LearnedAction::Left).visits == 180);
  OnlineLearning demo;
  for (int i = 0; i < 200; ++i) demo.imitate (state, LearnedAction::Right);
  CHECK (demo.choose (state, AllLearningActions, seed, 0) == LearnedAction::Right);
  CHECK (demo.choose (state, AllLearningActions, seed, 0, false) == LearnedAction::Native);
  CHECK (demo.choose (state, 1u, seed, 0) == LearnedAction::Native);
  for (int i = 0; i < 100; ++i) CHECK (demo.choose (state, 1u << 2u, seed, 0.3f) == LearnedAction::Left);
  f.side = -120; CHECK (OnlineLearning::classify (f) == LearnedAction::Left);
  f.side = 0; f.forward = -120; CHECK (OnlineLearning::classify (f) == LearnedAction::Retreat);
  // Every supported observation maps to a separate, bounded state.
  bool seen[LearningStates] {}; size_t distinct {};
  for (unsigned team = 0; team < 2; ++team) for (unsigned phase = 0; phase < 3; ++phase)
    for (unsigned health = 0; health < 2; ++health) for (unsigned range = 0; range < 3; ++range)
      for (unsigned support = 0; support < 2; ++support) for (unsigned objective = 0; objective < 2; ++objective) {
        f.context.team = static_cast<uint8_t> (team); f.context.phase = static_cast<uint8_t> (phase);
        f.health = health ? 20 : 100; f.context.visible = range ? 1 : 0; f.context.enemyDistance = range == 1 ? 200 : 600;
        f.context.friends = static_cast<uint8_t> (support); f.context.objective = static_cast<uint8_t> (objective);
        const auto s = demo.state (f, false); CHECK (s < LearningStates); CHECK (!seen[s]); seen[s] = true; ++distinct;
      }
  CHECK (distinct == LearningStates / 3);
  // Only currently valid actions may contribute bootstrapped value.
  OnlineLearning masked; masked.reinforce (1, LearnedAction::Retreat, 4, 1, true);
  masked.reinforce (0, LearnedAction::Native, 0, 1, false, false, 1u);
  CHECK (masked.cell (0, LearnedAction::Native).recent == 0);
  masked.reinforce (0, LearnedAction::Native, 0, 1, false);
  CHECK (masked.cell (0, LearnedAction::Native).recent > 0);
  auto before = masked.stats ().updates;
  masked.reinforce (0, LearnedAction::Native, std::numeric_limits<float>::quiet_NaN (), 0, true);
  masked.reinforce (65535, LearnedAction::Native, 1, 0, true);
  CHECK (masked.stats ().updates == before);
  for (int i = 0; i < 500; ++i) l.observeOpponent (0, 0, true);
  CHECK (l.pattern (0) == 2);
  const auto changes = l.opponent (0).changes;
  for (int i = 0; i < 40; ++i) l.observeOpponent (0, 1, false);
  CHECK (l.pattern (0) == 1 && l.opponent (0).changes > changes);
  CHECK (l.opponent (1).observations == 0);
  f.context.team = 0; CHECK (l.state (f) % 3u == 1u); CHECK (l.state (f, false) % 3u == 0u);
  // Team bandit learns the actual selected formation, credits a result only once.
  CHECK (l.chooseTactic (0, seed, 0) == TeamTactic::Split);
  l.finishTeam (0, -1); const auto results = l.stats ().teamRounds; l.finishTeam (0, -1);
  CHECK (l.stats ().teamRounds == results);
  l.newRound (); CHECK (l.chooseTactic (0, seed, 0) == TeamTactic::PressureA);
  l.finishTeam (0, 1); l.newRound (); CHECK (l.chooseTactic (0, seed, 0) == TeamTactic::PressureA);
  CHECK (l.tactic (1, TeamTactic::Split).visits == 0);
  std::string error; const char *path = "learning-test.hlearn";
  CHECK (l.save (path, "de_learning", 123, error)); CHECK (!l.dirty ());
  OnlineLearning restored; CHECK (restored.load (path, "de_learning", 123, error));
  CHECK (restored.stats ().updates == l.stats ().updates);
  CHECK (restored.cell (state, LearnedAction::Left).visits == 180);
  CHECK (restored.cell (state, LearnedAction::Left).lifetime == l.cell (state, LearnedAction::Left).lifetime);
  CHECK (restored.opponent (0).changes == l.opponent (0).changes);
  const auto good = read (path);
  CHECK (!restored.load (path, "de_other", 123, error));
  CHECK (!restored.load (path, "de_learning", 124, error));
  CHECK (restored.stats ().updates == l.stats ().updates);
  auto bad = good; bad[100] ^= 32; write (path, bad);
  CHECK (!restored.load (path, "de_learning", 123, error));
  CHECK (restored.stats ().updates == l.stats ().updates);
  // A valid checksum cannot authorize NaN/Inf Q values.
  bad = good; const auto offset = 24 + std::string ("de_learning").size ();
  bad[offset] = 0; bad[offset + 1] = 0; bad[offset + 2] = 0x80; bad[offset + 3] = 0x7f;
  auto hash = hashBytes (bad.data (), bad.size () - 4);
  for (unsigned i = 0; i < 4; ++i) bad[bad.size () - 4 + i] = static_cast<uint8_t> (hash >> (i * 8u));
  write (path, bad); CHECK (!restored.load (path, "de_learning", 123, error));
  for (size_t length : {size_t (0), size_t (8), size_t (100), good.size () - 1}) {
    write (path, std::vector<uint8_t> (good.begin (), good.begin () + length));
    CHECK (!restored.load (path, "de_learning", 123, error));
  }
  // A failed write does not clear dirty state or destroy the last successful file.
  write (path, good); restored.reinforce (state, LearnedAction::Hold, 1, 0, true);
  CHECK (!restored.save ("missing-learning-dir/file.hlearn", "de_learning", 123, error));
  CHECK (restored.dirty () && read (path) == good);
  CHECK (restored.save (path, "de_learning", 123, error));
  OnlineLearning second; CHECK (second.load (path, "de_learning", 123, error));
  CHECK (second.stats ().updates == restored.stats ().updates);
  std::remove (path);
  std::cout << "PASS: reward learning, policy reversal, imitation, self-play provenance, safe action masks, state separation, drift, team credit, durable storage and malformed input\n";
}
