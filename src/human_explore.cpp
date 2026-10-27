// Autonomous navigation through a geometry-discovered overlay. GPL-2.0-or-later.
#include "copycat/copycat_explore.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <aibo.h>

namespace bot {
namespace {
rbcc::ExploreMap space;
struct Explorer {
  std::vector<uint16_t> path {};
  size_t cursor {};
  int serial {-1}, user {-1}, visited {-1}, previous {-1}, goal {-1};
  float next_plan {}, next_visit {}, progress {}, best {1e30f}, hold {}, retry {};
  ystl::Vector watch {};
};
Explorer explorers[kGameMaxPlayers];
std::string explore_path, explore_map;
uint32_t physics {}; bool started {}; float next_scan {}, plan_slot {};
unsigned scans {}, failures {}, arrivals {}, trace_count {};
rbcc::Vec Core (const ystl::Vector &v) { return {v.x,v.y,v.z}; }
ystl::Vector Engine (const rbcc::Vec &v) { return {v.x,v.y,v.z}; }
ystl::Vector Feet (const edict_t *e) { return e->v.origin - ystl::Vector (0,0,(e->v.flags & FL_DUCKING) ? 18.0f : 36.0f); }
bool Clear (const TraceResult &r) { return !r.fAllSolid && !r.fStartSolid && r.flFraction >= 0.999f; }
void Hull (edict_t *e, const ystl::Vector &a, const ystl::Vector &b, bool crouch, TraceResult &tr) {
  ++trace_count; engfuncs.pfnTraceHull (a,b,1,crouch ? head_hull : human_hull,e,&tr);
}
bool Floor (edict_t *e, const ystl::Vector &p, ystl::Vector &out) {
  TraceResult tr {}; ++trace_count;
  engfuncs.pfnTraceLine (p + ystl::Vector (0,0,20), p - ystl::Vector (0,0,64), 1, e, &tr);
  if (tr.fAllSolid || tr.fStartSolid || tr.flFraction >= 0.999f || tr.vecPlaneNormal.z < 0.65f) return false;
  out = tr.vecEndPos;
  const int contents = engfuncs.pfnPointContents (out + ystl::Vector (0,0,8));
  return contents != CONTENTS_WATER && contents != CONTENTS_SLIME && contents != CONTENTS_LAVA && contents != CONTENTS_SKY;
}
bool BodySegment (edict_t *e, const ystl::Vector &a, const ystl::Vector &b, bool duck) {
  const float center = duck ? 18.0f : 36.0f;
  const auto from = a + ystl::Vector (0,0,center), to = b + ystl::Vector (0,0,center);
  TraceResult tr {}; Hull (e, to, to, duck, tr); if (!Clear (tr)) return false;
  Hull (e, from, to, duck, tr); if (Clear (tr)) return true;
  // Let the game step over ordinary stairs; never manufacture a jump or velocity.
  const float step = std::clamp (engfuncs.pfnCVarGetFloat ("sv_stepsize"), 0.0f, 18.0f);
  if (step < 1.0f) return false;
  Hull (e, from, from + ystl::Vector (0,0,step), duck, tr); if (!Clear (tr)) return false;
  Hull (e, from + ystl::Vector (0,0,step), to + ystl::Vector (0,0,step), duck, tr); if (!Clear (tr)) return false;
  Hull (e, to + ystl::Vector (0,0,step), to, duck, tr); return Clear (tr);
}
bool Probe (edict_t *e, const rbcc::Vec &from, const rbcc::Vec &candidate, rbcc::ExploreStep &result, bool sight = true) {
  const auto a = Engine (from), target = Engine (candidate); const auto delta = target - a;
  if (delta.length2d () > 130 || std::fabs (delta.z) > 96) return false;
  const int steps = std::clamp (static_cast<int> (std::ceil (delta.length2d () / 16.0f)), 1, 8);
  ystl::Vector last = a; bool duck = false, reverse = true;
  for (int i = 1; i <= steps; ++i) {
    auto expected = a + delta * (static_cast<float> (i) / static_cast<float> (steps)); expected.z = last.z;
    ystl::Vector floor {};
    if (!Floor (e, expected, floor)) return false;
    const float rise = floor.z - last.z;
    if (rise > 18.1f || rise < -32.0f) return false;
    if (rise < -18.1f) reverse = false;
    if (!BodySegment (e,last,floor,duck)) {
      if (duck || !BodySegment (e,last,floor,true)) return false;
      duck = true;
    }
    last = floor;
  }
  result.foot = Core (last); result.posture = duck ? 1 : 0; result.reverse = reverse;
  if (sight && space.nearest (result.foot, 34) < 0) {
    const auto eye = last + ystl::Vector (0,0,duck ? 30.0f : 54.0f); float open = 0;
    for (auto direction : {ystl::Vector (1,0,0),ystl::Vector (-1,0,0),ystl::Vector (0,1,0),ystl::Vector (0,-1,0)}) {
      TraceResult tr {}; ++trace_count; engfuncs.pfnTraceLine (eye, eye + direction * 1000, 1, e, &tr);
      if (!tr.fAllSolid && !tr.fStartSolid) open = std::max (open, tr.flFraction);
      if (HumanArena::Enabled () && tr.flFraction < 0.8f) {
        for (const float pitch : {-6.0f,-12.0f,6.0f,12.0f}) {
          const auto slope = ystl::Vector (pitch,direction.angles ().y,0).forward ();
          TraceResult inclined {}; ++trace_count;
          engfuncs.pfnTraceLine (eye,eye+slope*1000,1,e,&inclined);
          if (!inclined.fAllSolid && !inclined.fStartSolid) open = std::max (open,inclined.flFraction);
        }
      }
    }
    result.vantage = static_cast<uint8_t> (std::clamp (open * 255.0f, 0.0f, 255.0f));
  }
  return true;
}
bool Alive (const Bot &bot) { return bot.is_alive_ && !bot.Ent ()->free && bot.pev->health > 0 && bot.pev->deadflag == DEAD_NO; }
}
void HumanExplore::Cancel (Bot &bot, float pause) {
  auto &x = explorers[bot.Index ()]; space.release (static_cast<unsigned> (bot.Index ()));
  const bool had_path = !x.path.empty (); x.path.clear (); x.cursor = 0; x.goal = -1; x.hold = 0; x.best = 1e30f;
  x.next_plan = game.Time () + pause;
  if (had_path) { bot.ClearSearchNodes (); bot.current_node_index_ = kInvalidNodeIndex; }
}
void HumanExplore::Start (const char *map, uint32_t env, const char *data) {
  Stop (); explore_map = map; physics = env;
  const std::string base = data; explore_path = base.substr (0,base.size () - 5) + ".hexplore";
  started = true; next_scan = plan_slot = 0; scans = failures = arrivals = trace_count = 0;
  std::string error; std::error_code ec;
  if (cv_explore_save && !space.load (explore_path, explore_map, physics, error) && std::filesystem::exists (explore_path, ec))
    game.Print ("[Explore] Ignored saved overlay: %s", error.c_str ());
}
void HumanExplore::Stop () { if (started) Save (); started = false; space.clear (); for (auto &x : explorers) x = Explorer {}; }
bool HumanExplore::Save () {
  if (!started || !cv_explore_save || !space.dirty ()) return true;
  std::error_code ec; std::filesystem::create_directories (std::filesystem::path (explore_path).parent_path (),ec);
  std::string error;
  if (ec || !space.save (explore_path, explore_map, physics, error)) { game.Print ("[Explore] Save failed: %s", ec ? ec.message ().c_str () : error.c_str ()); return false; }
  return true;
}
bool HumanExplore::Dirty () { return started && cv_explore_save && space.dirty (); }
void HumanExplore::Reset (int slot) { if (slot >= 0 && slot < kGameMaxPlayers) { space.release (static_cast<unsigned> (slot)); explorers[slot] = Explorer {}; } }
void HumanExplore::NewRound () { if (started) { space.newRound (); for (int i = 0; i < kGameMaxPlayers; ++i) Reset (i); } }
bool HumanExplore::Scout (const Bot &bot) {
  if (!started || !HumanCopycat::Enabled () || !cv_explore || !Alive (bot) || bot.has_c4_ || bot.is_vip_ || bot.has_hostage_ ||
      bot.is_creature_ || (bot.team_ != Team::CT && bot.team_ != Team::Terrorist) ||
      game_state.IsBombPlanted () || game_state.IsRoundOver () || cv_debug_goal.As<int> () >= 0) return false;
  if (HumanArena::PatrolMode (bot)) return HumanArena::PatrolDuty (bot) && !HumanArena::PatrolActive (bot);
  struct Candidate { uint32_t rank; int slot; }; std::vector<Candidate> roster;
  for (const auto &b : bots) if (Alive (b) && b.team_ == bot.team_ && !b.has_c4_ && !b.is_vip_ && !b.has_hostage_) {
    uint32_t mix = static_cast<uint32_t> (b.Index () + 1) * 2654435761u ^ HumanCopycat::RoundNumber () * 2246822519u;
    mix = rbcc::hashBytes (&mix, sizeof mix); roster.push_back ({mix,b.Index ()});
  }
  std::sort (roster.begin (), roster.end (), [] (const Candidate &a,const Candidate &b) { return a.rank != b.rank ? a.rank < b.rank : a.slot < b.slot; });
  const int count = static_cast<int> (roster.size ()); if (!count) return false;
  int selected = (count * cv_explore_percent.As<int> () + 99) / 100;
  selected = std::min (selected, count > 1 ? count - 1 : count);
  for (int i = 0; i < selected; ++i) if (roster[static_cast<size_t> (i)].slot == bot.Index ()) return true;
  return false;
}
bool HumanExplore::Active (const Bot &bot) { const auto &x = explorers[bot.Index ()]; return started && cv_explore && !x.path.empty (); }
void HumanExplore::Frame () {
  if (!started || !HumanCopycat::Enabled () || !cv_explore || game.Time () < next_scan || cv_freeze_bots || game_state.IsRoundOver ()) return;
  next_scan = game.Time () + 0.05f; edict_t *observer = nullptr;
  for (auto &bot : bots) if (Alive (bot) && bot.IsOnFloor () && !bot.IsOnLadder () && !bot.IsInWater ()) {
    auto &x = explorers[bot.Index ()]; auto e = bot.Ent (); const int user = engfuncs.pfnGetPlayerUserId (e);
    if (x.serial != e->serialnumber || x.user != user) { Reset (bot.Index ()); x.serial = e->serialnumber; x.user = user; }
    const auto foot = Feet (e); ystl::Vector floor {};
    if (game.Time () >= x.next_visit && Floor (e,foot,floor) && std::fabs (floor.z - foot.z) < 20) {
      x.next_visit = game.Time () + 0.4f; const int node = space.seed (Core (floor), (e->v.flags & FL_DUCKING) ? 1 : 0);
      if (node >= 0 && game.Time () >= x.retry) { space.retry (node); x.retry = game.Time () + 4.0f + static_cast<float> (bot.Index () % 5) * 0.1f; }
      if (node >= 0 && node != x.visited) {
        if (x.visited >= 0 && rbcc::distance (space.nodes ()[static_cast<size_t> (x.visited)].foot, Core (floor)) < 128) {
          rbcc::ExploreStep step;
          if (Probe (e, space.nodes ()[static_cast<size_t> (x.visited)].foot, Core (floor), step, false)) space.link (x.visited,node,step.posture,step.reverse);
        }
        space.visit (node,static_cast<unsigned> (bot.team_)); x.visited = node;
      }
    }
    if (!observer && Scout (bot)) observer = e;
  }
  if (!observer) return;
  const auto began = std::chrono::steady_clock::now ();
  scans += space.expand (static_cast<unsigned> (cv_explore_probes.As<int> ()), [observer] (const rbcc::Vec &a, const rbcc::Vec &b, rbcc::ExploreStep &step) { return Probe (observer,a,b,step); },
    [&] { return std::chrono::steady_clock::now () - began > std::chrono::milliseconds (2); });
}
bool HumanExplore::Move (Bot &bot, HumanCommand &cmd, ystl::Vector &desired, bool available) {
  auto &x = explorers[bot.Index ()];
  const bool task = bot.GetTaskId () == TaskId::Normal || bot.GetTaskId () == TaskId::Camp || bot.GetTaskId () == TaskId::Pause;
  const float elapsed = game.Time () - game_state.GetRoundStartTime ();
  const bool urgent = game.MapIs (MapFlags::Demolition | MapFlags::HostageRescue | MapFlags::Assassination) &&
    elapsed > 0 && game_state.GetRoundEndTime () - game.Time () < 30;
  if (!available || HumanArena::VantageHolding (bot) || !Scout (bot) || !task || urgent || bot.pev->health < 30 || !bot.IsOnFloor () || bot.IsOnLadder () || bot.IsInWater () ||
      has_flag (bot.states_, Sense::SeeingEnemy | Sense::HearingEnemy)) { Cancel (bot,0.5f); return false; }
  const auto foot = Core (Feet (bot.Ent ()));
  if (x.path.empty ()) {
    if (game.Time () < x.next_plan || game.Time () < plan_slot) return false;
    x.next_plan = game.Time () + 1.0f; plan_slot = game.Time () + (HumanArena::Enabled () ? 0.1f : 0.05f);
    const std::function<float (const rbcc::Vec &)> bias = HumanArena::Enabled () ? std::function<float (const rbcc::Vec &)> ([&] (const rbcc::Vec &p) { return HumanArena::PlaceBias (bot,Engine (p)); }) : std::function<float (const rbcc::Vec &)> {};
    const std::function<bool (const rbcc::Vec &)> fresh = HumanArena::Enabled () ? std::function<bool (const rbcc::Vec &)> ([&] (const rbcc::Vec &p) { return HumanArena::Fresh (bot,Engine (p)); }) : std::function<bool (const rbcc::Vec &)> {};
    auto plan = space.plan (foot, static_cast<unsigned> (bot.team_), static_cast<unsigned> (bot.Index ()),
      HumanCopycat::RoundNumber () * 2654435761u ^ static_cast<uint32_t> (bot.Index () + 1) ^ static_cast<uint32_t> (arrivals), bot.current_weapon_ == Weapon::AWP,
      HumanArena::Enabled () ? 4096u : 12000u,bias,fresh);
    if (plan.path.size () < 2) { if (game.Time () >= x.retry) { space.retry (space.nearest (foot)); x.retry = game.Time () + 3; } return false; }
    x.path = std::move (plan.path); x.cursor = 0; x.goal = plan.goal; x.previous = -1; x.progress = game.Time (); x.best = 1e30f;
    bot.ClearSearchNodes (); bot.ClearTask (TaskId::Camp); bot.ClearTask (TaskId::Pause);
  }
  while (x.cursor < x.path.size () && rbcc::distance (foot,space.nodes ()[x.path[x.cursor]].foot) < 22) {
    x.previous = x.path[x.cursor++]; space.visit (x.previous, static_cast<unsigned> (bot.team_)); x.progress = game.Time (); x.best = 1e30f;
  }
  if (x.cursor >= x.path.size ()) {
    ystl::Vector sniper_watch {};
    const float sniper_score = bot.current_weapon_ == Weapon::AWP && x.goal >= 0 ?
      HumanArena::SniperWatch (bot,Engine (space.nodes ()[static_cast<size_t> (x.goal)].foot),sniper_watch) : 0;
    if (bot.current_weapon_ == Weapon::AWP && x.goal >= 0 && (space.nodes ()[static_cast<size_t> (x.goal)].vantage >= 160 || sniper_score >= 160)) {
      if (x.hold <= 0.0f) {
        if (!HumanArena::Fresh (bot,Engine (space.nodes ()[static_cast<size_t> (x.goal)].foot))) { Cancel (bot,1.0f); return false; }
        HumanArena::Visit (bot,Engine (space.nodes ()[static_cast<size_t> (x.goal)].foot),3);
        x.hold = game.Time () + 2.0f + static_cast<float> ((static_cast<unsigned> (bot.Index ()) + HumanCopycat::RoundNumber ()) % 4u);
        const auto eye = bot.GetEyesPos (); float longest = -1;
        for (auto direction : {ystl::Vector (1,0,0),ystl::Vector (-1,0,0),ystl::Vector (0,1,0),ystl::Vector (0,-1,0)}) {
          TraceResult tr {}; ++trace_count; engfuncs.pfnTraceLine (eye,eye + direction * 1200,1,bot.Ent (),&tr);
          if (!tr.fAllSolid && !tr.fStartSolid && tr.flFraction > longest) { longest = tr.flFraction; x.watch = direction.angles (); }
        }
        if (sniper_score > 0) x.watch = sniper_watch;
      }
      if (game.Time () < x.hold) {
        cmd.forward = cmd.side = cmd.up = 0; cmd.buttons &= static_cast<uint16_t> (~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
        desired = x.watch; return true;
      }
    }
    if (x.goal >= 0) HumanArena::Visit (bot,Engine (space.nodes ()[static_cast<size_t> (x.goal)].foot),0);
    ++arrivals; Cancel (bot,0.2f); return false;
  }
  const auto node = x.path[x.cursor]; const auto &target = space.nodes ()[node]; const float distance = rbcc::distance (foot,target.foot);
  if (distance < x.best - 2) { x.best = distance; x.progress = game.Time (); }
  rbcc::ExploreStep step;
  if (game.Time () - x.progress > 2.0f || !Probe (bot.Ent (),foot,target.foot,step,false) || rbcc::distance (step.foot,target.foot) > 18) {
    space.block (x.previous,node); space.retry (x.previous); ++failures; Cancel (bot,2); return false;
  }
  const auto point = Engine (target.foot); auto heading = (point - Feet (bot.Ent ())).angles (); heading.x = heading.z = 0;
  cmd.angles = heading; cmd.forward = std::min (bot.pev->maxspeed,distance < 40 ? 110.0f : 220.0f); cmd.side = cmd.up = 0;
  cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2 | IN_JUMP | IN_DUCK | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
  cmd.buttons |= IN_FORWARD; if (step.posture || target.posture) cmd.buttons |= IN_DUCK;
  desired = (point + ystl::Vector (0,0,54) - bot.GetEyesPos ()).angles (); desired.x = -desired.x;
  bot.IgnoreCollision (); return true;
}
void HumanExplore::Status () {
  unsigned active = 0; for (const auto &b : bots) if (Active (b)) ++active;
  game.Print ("[Explore] enabled=%d nodes=%u edges=%u active=%u probes=%u traces=%u arrivals=%u blocked=%u capacity=%u",
    cv_explore.As<int> (),static_cast<unsigned> (space.nodes ().size ()),static_cast<unsigned> (space.edges ()),active,scans,trace_count,arrivals,failures,static_cast<unsigned> (rbcc::ExploreMap::MaxNodes));
}
bool HumanExplore::Walkable (Bot &b,const ystl::Vector &a,const ystl::Vector &c) { rbcc::ExploreStep step; return Probe (b.Ent (),Core (a),Core (c),step,false) && rbcc::distance (step.foot,Core (c)) < 20.0f; }
#if defined(YSTL_TESTS)
size_t HumanExplore::TestNodes () { return space.nodes ().size (); }
size_t HumanExplore::TestEdges () { return space.edges (); }
int HumanExplore::TestTarget (const Bot &b) { return explorers[b.Index ()].goal; }
ystl::Vector HumanExplore::TestFoot (int n) { return n >= 0 && static_cast<size_t> (n) < space.nodes ().size () ? Engine (space.nodes ()[static_cast<size_t> (n)].foot) : ystl::Vector {}; }
bool HumanExplore::TestProbe (Bot &b, const ystl::Vector &a,const ystl::Vector &c) { rbcc::ExploreStep s; return Probe (b.Ent (),Core (a),Core (c),s,false); }
#endif
} // namespace bot
