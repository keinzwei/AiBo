// AiBo Human Copycat 4.2.6. GPL-2.0-or-later; see COPYING.GPL-2.0.
#pragma once

namespace rbcc { struct Clip; }
namespace bot {
class Bot;

// Intent enters here, one ordinary GoldSrc command leaves here. No physics writes.
struct HumanCommand {
  ystl::Vector angles {};
  float forward {}, side {}, up {};
  uint16_t buttons {};
  uint8_t impulse {}, msec {};
};

class HumanCopycat final {
public:
  static bool Enabled ();
  static void LevelStart ();
  static void LevelStop ();
  static void Frame ();
  static void Record (const edict_t *player, const usercmd_t *cmd);
  static void Blind (edict_t *player, float seconds);
  static void ResetBot (Bot &bot);
  static void BeforeThink (Bot &bot);
  static void NewRound ();
  static unsigned RoundNumber ();
  static int StrategicGoal (Bot &bot);
  static bool SpreadActive ();
  static bool MayAssist (const Bot &bot, const ystl::Vector &origin);
  static float RouteWeight (Team team, int node);
  static bool NavigatingHuman (const Bot &bot);
  static bool Navigate (Bot &bot, HumanCommand &cmd, ystl::Vector &desired);
  static void Finalize (Bot &bot, HumanCommand &cmd);
  static bool VisualPoint (edict_t *observer, edict_t *target, ystl::Vector &point, bool headFirst = false);
  static bool HasWitness (Team team, edict_t *target);
  static bool SmokeCover (const ystl::Vector &point);
  static void Status ();
  static void Save ();
  static void Reload ();
  static void LearningDamage (edict_t *victim, edict_t *attacker, int health, int armor);
  static void LearningDeath (edict_t *killer, edict_t *victim);
  static void LearningRoundEnd (Team winner);
  static void LearningStatus ();
#if defined(YSTL_TESTS)
  static uint32_t TestAddClip (const rbcc::Clip &clip);
  static size_t TestClipCount ();
  static bool TestReplaying (const Bot &bot);
  static uint32_t TestEnvironment ();
  static const char *TestDataPath ();
  static size_t TestHumanNodes ();
  static void TestRouteBudget (unsigned milliseconds);
  static int TestRole (const Bot &bot);
  static uint32_t TestLearningUpdates ();
  static uint32_t TestLearningHumanSamples ();
  static uint32_t TestLearningSelfPlay ();
  static uint32_t TestLearningTeamRounds ();
  static int TestLearningAction (const Bot &bot);
  static uint32_t TestLearningObservations (Team team);
#endif
};
} // namespace bot
