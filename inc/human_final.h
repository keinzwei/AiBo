// FINAL 4.3.0: actor decisions, competitive moods, automatic physical boosts, lobby pacing.
// GPL-2.0-or-later.
#pragma once
namespace bot {
class HumanArena final {
  static bool TryBoost (Bot &bot);

public:
  static bool Enabled ();
  static void Start (const char *map, uint32_t environment, const char *data);
  static void Stop ();
  static bool Save ();
  static bool Dirty ();
  static void NewRound ();
  static void BeforeThink (Bot &bot);
  static void ResetBot (Bot &bot);
  static void Tune (Bot &bot);
  static int Goal (Bot &bot);
  static bool PatrolMode (const Bot &bot);
  static bool PatrolDuty (const Bot &bot);
  static bool PatrolActive (const Bot &bot);
  static int PatrolRegion (const ystl::Vector &foot);
  static int PatrolGoal (Bot &bot);
  static bool Camp (Bot &bot, bool hide = false);
  static bool Fresh (const Bot &bot, const ystl::Vector &foot);
  static float PlaceBias (const Bot &bot, const ystl::Vector &foot);
  static void Visit (Bot &bot, const ystl::Vector &foot, int use);
  static void IdleLook (Bot &bot, ystl::Vector &desired);
  static float SniperWatch (Bot &bot, const ystl::Vector &foot, ystl::Vector &desired);
  static bool ObjectiveSmoke (Bot &bot, bool defuse);
  static bool Motor (Bot &bot, HumanCommand &command, ystl::Vector &desired, bool available);
  static void CancelBoost (int slot);
  static bool Boosting (const Bot &bot);
  static bool VantageHolding (Bot &bot);
  static int Mood (const Bot &bot);
  static bool Top (const Bot &bot);
  static int Rank (const Bot &bot);
  static float Risk (const Bot &bot);
  static void Damage (edict_t *victim, edict_t *attacker, int damage);
  static void Death (edict_t *killer, edict_t *victim);
  static bool LobbyEnabled ();
  static int Capacity ();
  static bool ReadyToJoin ();
  static int LobbyTarget (int desired, int current);
  static bool MayLeave (const Bot &bot);
  static void Leaving (Bot &bot);
  static void Joined (Bot &bot);
  static void Status ();
#if defined(YSTL_TESTS)
  static bool TestStartBoost (Bot &helper, Bot &climber, const ystl::Vector &base, const ystl::Vector &landing);
  static unsigned TestBoosts ();
  static unsigned TestRepeats ();
  static unsigned TestLeaves ();
  static bool TestBoostPoints (const Bot &bot, ystl::Vector &base, ystl::Vector &approach, ystl::Vector &landing);
  static void TestNextJoin (float when);
#endif
};
}
