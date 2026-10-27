// Autonomous navigation through a geometry-discovered overlay. GPL-2.0-or-later.
#pragma once
namespace bot {
class HumanExplore final {
  static void Cancel (Bot &bot, float pause);
public:
  static void Start (const char *map, uint32_t environment, const char *data_path);
  static void Stop ();
  static void Frame ();
  static bool Save ();
  static bool Dirty ();
  static void Reset (int slot);
  static void NewRound ();
  static bool Scout (const Bot &bot);
  static bool Active (const Bot &bot);
  static bool Move (Bot &bot, HumanCommand &command, ystl::Vector &desired, bool available);
  static void Status ();
  static bool Walkable (Bot &bot,const ystl::Vector &from_foot,const ystl::Vector &to_foot);
#if defined(YSTL_TESTS)
  static size_t TestNodes ();
  static size_t TestEdges ();
  static int TestTarget (const Bot &bot);
  static ystl::Vector TestFoot (int node);
  static bool TestProbe (Bot &bot, const ystl::Vector &from_foot, const ystl::Vector &to_foot);
#endif
};
}
