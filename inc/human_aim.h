// FINAL 4.3.3 adaptive HUMAN AIM. GPL-2.0-or-later.
#pragma once
namespace rbcc {
struct Frame;
}
namespace bot {
class HumanAimCoach final {
public:
  static bool Enabled ();
  static void Start (const char *map, uint32_t environment, const char *data);
  static void Stop ();
  static bool Save ();
  static bool Dirty ();
  static void ResetBot (Bot &bot);
  static void RecordHuman (edict_t *human, const rbcc::Frame &frame);
  static void WeaponUpdate (edict_t *player, int active, unsigned weapon, int clip);
  static void Damage (edict_t *victim, edict_t *attacker, int damage, int bits);
  static void Death (edict_t *killer, edict_t *victim, bool head);
  static void Heard (Bot &bot, edict_t *source, const ystl::Vector &point, float started);
  static float Update (Bot &bot, edict_t *visibleOpponent);
  static float Level (const Bot &bot);
  static float Practice (const Bot &bot);
  static bool PreferHead (Bot &bot, uint32_t target);
  static void Seen (Bot &bot, uint32_t token, const ystl::Vector &point);
  static bool Wallbang (Bot &bot, ystl::Vector &point, uint32_t &token);
  static void Prepare (Bot &bot, HumanCommand &command, ystl::Vector &desired, bool visible, bool wallbang, bool available, bool motor);
  static bool Finish (Bot &bot, HumanCommand &command, bool visible, bool wallbang, bool available, bool aimAllows, float distance);
  static void Status ();
#if defined(YSTL_TESTS)
  static unsigned TestShots (edict_t *human);
  static unsigned TestHits (edict_t *human);
  static unsigned TestHeadKills (edict_t *human);
  static float TestEstimate (edict_t *human);
  static ystl::Vector TestContact (const Bot &bot);
#endif
};
}
