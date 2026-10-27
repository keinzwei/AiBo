// AiBo Human Copycat 4.3.3 FINAL. GPL-2.0-or-later.
// RealBot supplies the bounded RBCC library; AiBo retains navigation/tactics.
#include "copycat/copycat_core.h"
#include "copycat/copycat_routes.h"
#include "copycat/copycat_learning.h"
#include "copycat/copycat_aim.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <aibo.h>

namespace bot {
namespace {
rbcc::Database database;
rbcc::HumanRoutes human_routes;
rbcc::OnlineLearning learning;
struct LearningActor {
  int serial {-1}, user {-1};
  uint8_t team {255};
  uint16_t state {};
  rbcc::LearnedAction action {rbcc::LearnedAction::Native};
  uint32_t target {}, random {1};
  float reward {}, started {}, next {}, next_observation {}, next_imitation {};
  bool pending {}, selfplay {}, train {};
};
LearningActor learners[kGameMaxPlayers];
bool learning_round_finished {}, team_experience[2] {}, death_seen[kGameMaxPlayers] {};
std::string learning_path;
#if defined(YSTL_TESTS)
unsigned route_budget_ms {2};
#else
constexpr unsigned route_budget_ms {2};
#endif
struct Actor {
  rbcc::HumanAim aim;
  rbcc::HumanStress stress;
  rbcc::Playback replay;
  unsigned frame_elapsed {};
  std::vector<uint16_t> route {};
  size_t route_cursor {};
  float next_route {}, route_progress {}, route_distance {}, hold_until {}, next_cover_check {};
  bool observed_cover {};
  int hide_node {-1};
  bool route_active {};
  float next_match {}, last_time {}, blind_until {};
  uint32_t exclude {}, replay_target {};
  int serial {-1}, user_id {-1};
  bool initialized {};
  float brake_until {}, next_brake {}, duck_start {}, next_duck {};
  bool double_duck {};
};
Actor actors[kGameMaxPlayers];
rbcc::Recorder recorders[kGameMaxPlayers];
struct Projectile {
  edict_t *entity {};
  int serial {}, owner {-1};
  uint8_t weapon {};
  ystl::Vector last {};
};
Projectile projectiles[128];
struct Cloud { ystl::Vector origin {}; float expires {}; edict_t *entity {}; int serial {}; bool detonated {}; };
Cloud clouds[64];
std::string map_name, data_path, navigation_path;
struct Assignment { int role {-1}, previous {-1}, goal {-1}, middle {-1}; bool middle_done {}; } assignments[kGameMaxPlayers];
struct TeamSplit { bool ready {}; uint32_t roster {}; } split_teams[2];
uint32_t round_number {}, route_seed {1};
uint16_t native_visits[2][4096] {};
int last_native_node[kGameMaxPlayers] {};

uint32_t bsp_hash {}, environment {};
float next_save {}, next_environment {}, next_projectiles {};
uint64_t commands {}, completed {}, aborted {}, recorded {};
bool ready {}, dirty {}, commands_registered {};

rbcc::Vec ToCore (const ystl::Vector &p) { return {p.x, p.y, p.z}; }
ystl::Vector ToEngine (const rbcc::Vec &p) { return {p.x, p.y, p.z}; }
float Clamp (float v, float lo, float hi) { return std::clamp (v, lo, hi); }
int Slot (const edict_t *ent) {
  if (!ent) return -1;
  const int index = game.IndexOfPlayer (ent);
  return index >= 0 && index < kGameMaxPlayers ? index : -1;
}
bool Alive (const edict_t *e) {
  return e && !e->free && (e->v.flags & FL_CLIENT) && e->v.deadflag == DEAD_NO && e->v.health > 0.0f;
}
uint32_t Profile () {
  if (!cv_human_profile.ptr) return 0;
  return static_cast<uint32_t> (std::clamp (std::strtod (cv_human_profile.ptr->string, nullptr), 0.0, 4294967295.0));
}
uint32_t Token (const edict_t *e) {
  return e ? (static_cast<uint32_t> (game.IndexOfEntity (e)) << 16u) ^ (static_cast<uint32_t> (e->serialnumber) & 65535u) : 0u;
}
uint8_t WeaponId (const edict_t *e) {
  struct Name { uint8_t id; const char *model; };
  static constexpr Name names[] = {
    {1,"p228"},{3,"scout"},{4,"hegrenade"},{5,"xm1014"},{6,"c4"},{7,"mac10"},{8,"aug"},
    {9,"smokegrenade"},{10,"elite"},{11,"fiveseven"},{12,"ump45"},{13,"sg550"},{14,"galil"},
    {15,"famas"},{16,"usp"},{17,"glock18"},{18,"awp"},{19,"mp5"},{20,"m249"},{21,"m3"},
    {22,"m4a1"},{23,"tmp"},{24,"g3sg1"},{25,"flashbang"},{26,"deagle"},{27,"sg552"},{28,"ak47"},
    {29,"knife"},{30,"p90"}
  };
  for (const auto &n : names) {
    char model[80]; std::snprintf (model, sizeof model, "models/v_%s.mdl", n.model);
    if (std::strcmp (e->v.viewmodel.chars (), model) == 0) return n.id;
  }
  return 0;
}
bool SmokeBlocks (const ystl::Vector &from, const ystl::Vector &to) {
  const auto delta = to - from;
  const float length_sq = delta.length_sq ();
  if (length_sq < 0.001f) return false;
  for (const auto &cloud : clouds) {
    if (cloud.expires <= game.Time ()) continue;
    const auto center = cloud.origin + ystl::Vector (0.0f, 0.0f, 24.0f);
    const float fraction = Clamp (((center - from) | delta) / length_sq, 0.0f, 1.0f);
    if ((from + delta * fraction).distance_sq (center) < 150.0f * 150.0f) return true;
  }
  // Native detonation events also cover smoke entities removed by the game DLL.
  for (const auto &g : game_state.GetActiveGrenades ()) {
    if (g.kind != GrenadeKind::Smoke || g.origin.empty ()) continue;
    const float fraction = Clamp (((g.origin - from) | delta) / length_sq, 0.0f, 1.0f);
    if ((from + delta * fraction).distance_sq (g.origin) < 150.0f * 150.0f) return true;
  }
  return false;
}
bool ClearLine (edict_t *e, const ystl::Vector &point, edict_t *target = nullptr) {
  // Deliberately uncached. A wall/door closing between think and command must win.
  TraceResult result {};
  const auto eyes = e->v.origin + e->v.view_ofs;
  engfuncs.pfnTraceLine (eyes, point, 0, e, &result);
  return !result.fAllSolid && !result.fStartSolid &&
    (result.pHit == target && target != nullptr ? true : result.flFraction >= 0.999f);
}
rbcc::Context ContextFor (edict_t *e, edict_t *preferred = nullptr, bool acquire = true, float range = 2048.0f) {
  rbcc::Context c;
  const auto team = game.GetPlayerTeam (e);
  c.team = static_cast<uint8_t> (team);
  const float elapsed = game.Time () - game_state.GetRoundStartTime ();
  c.phase = elapsed < 20.0f ? 0 : (elapsed < 60.0f ? 1 : 2);
  c.objective = static_cast<uint8_t> ((game_state.IsBombPlanted () ? 1 : 0) |
    ((static_cast<uint32_t> (e->v.weapons) & (1u << 6u)) ? 2 : 0) |
    (game.IsPlayerVip (e) ? 4 : 0) | (game.MapIs (MapFlags::HostageRescue) ? 8 : 0));
  float nearest = range;
  ystl::Vector target_point {};
  edict_t *target = nullptr;
  for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i) {
    auto other = game.PlayerOfIndex (i);
    if (other == e || !Alive (other)) continue;
    const float distance = e->v.origin.distance (other->v.origin);
    if (game.GetPlayerTeam (other) == team && !game.Is (GameFlags::FreeForAll)) {
      if (distance < 600.0f) ++c.friends;
      continue;
    }
    if (!acquire || (preferred && preferred != other)) continue;
    if (distance >= nearest) continue;
    ystl::Vector point {};
    if (!HumanCopycat::VisualPoint (e, other, point)) continue;
    nearest = distance; target = other; target_point = point;
  }
  if (target) {
    c.visible = 1; c.enemy = ToCore (target_point); c.enemyDistance = nearest; c.targetToken = Token (target);
    const auto a = (target_point - (e->v.origin + e->v.view_ofs)).angles ();
    c.enemyYaw = rbcc::angleDelta (a.y, e->v.v_angle.y);
    c.enemyPitch = rbcc::angleDelta (-a.x, e->v.v_angle.x);
  }
  return c;
}
rbcc::Frame Snapshot (edict_t *e, bool acquire = true, float range = 2048.0f) {
  rbcc::Frame f;
  f.origin = ToCore (e->v.origin); f.velocity = ToCore (e->v.velocity);
  f.angles = ToCore (e->v.v_angle); f.punch = ToCore (e->v.punchangle);
  f.maxSpeed = e->v.maxspeed; f.weapon = WeaponId (e);
  f.health = static_cast<uint8_t> (Clamp (e->v.health, 0.0f, 255.0f));
  f.state = static_cast<uint8_t> (((e->v.flags & FL_ONGROUND) ? rbcc::Ground : 0) |
    ((e->v.flags & FL_DUCKING) ? rbcc::Crouched : 0) | (e->v.movetype == MOVETYPE_FLY ? rbcc::Ladder : 0) |
    (e->v.waterlevel > 1 ? rbcc::Water : 0));
  f.context = ContextFor (e, nullptr, acquire, range);
  return f;
}
uint32_t Fingerprint () {
  uint32_t hash = bsp_hash;
  static constexpr const char *vars[] = {"sv_gravity","sv_airaccelerate","sv_accelerate","sv_friction","sv_stopspeed",
    "sv_maxspeed","sv_stepsize","sv_maxvelocity","edgefriction","sv_wateraccelerate","sv_waterfriction","sv_bounce"};
  for (auto name : vars) {
    const auto value = engfuncs.pfnCVarGetString (name);
    hash = rbcc::hashBytes (name, std::strlen (name), hash);
    hash = rbcc::hashBytes (value ? value : "", std::strlen (value ? value : "") + 1u, hash);
  }
  return hash;
}
void ResetActors () {
  for (int i = 0; i < kGameMaxPlayers; ++i) { actors[i] = Actor {}; learners[i] = LearningActor {}; death_seen[i] = false; recorders[i].reset (); }
  for (auto &p : projectiles) p = Projectile {};
  for (auto &c : clouds) c = Cloud {};
}
void LoadEnvironment () {
  environment = Fingerprint ();
  char suffix[32]; std::snprintf (suffix, sizeof suffix, "-%08x.rbcc", environment);
  data_path = std::string (game.GetRunningModName ()) + "/addons/AiBo/data/copycat/" + map_name + suffix;
  database.clear (); dirty = false;
  std::string error;
  if (!database.load (data_path, map_name, environment, error)) {
    // Read-only legacy fallback. New saves always belong to this fork.
    const std::string legacy = "realbot/data/copycat/" + map_name + suffix;
    if (database.load (legacy, map_name, environment, error)) {
      dirty = true; game.Print ("[Human Copycat] Imported legacy RBCC: %s", legacy.c_str ());
    }
  }
  navigation_path = data_path.substr (0,data_path.size () - 5u) + ".hnav";
  learning_path = data_path.substr (0,data_path.size () - 5u) + ".hlearn";
  HumanExplore::Start (map_name.c_str (), environment, data_path.c_str ());
  HumanArena::Start (map_name.c_str (), environment, data_path.c_str ());
  HumanAimCoach::Start (map_name.c_str (), environment, data_path.c_str ());
  learning.clear ();
  if (cv_learning_lifelong && !learning.load (learning_path, map_name, environment, error) &&
      std::filesystem::exists (learning_path)) game.Print ("[Learning] Ignored invalid data: %s", error.c_str ());
  human_routes.clear ();
  if (!human_routes.load (navigation_path,map_name,environment,error)) {
    for (const auto &clip : database.clips ()) {
      human_routes.resetTeacher (0);
      for (const auto &f : clip.frames) human_routes.observe (f,0,true);
    }
  }
  ResetActors ();
  next_save = game.Time () + cv_human_autosave.As<float> ();
  next_environment = game.Time () + 1.0f;
}
// Only human-observed edges enter this overlay. Geometry, floors and current
// obstruction are validated again at the time a bot tries to use a segment.
bool HumanSegment (edict_t *e, const rbcc::Vec &a, const rbcc::Vec &b, uint8_t state) {
  const auto from = ToEngine (a), to = ToEngine (b);
  if ((state & rbcc::Water) || std::fabs (from.z - to.z) > 40.0f || from.distance_sq (to) > 210.0f * 210.0f) return false;
  TraceResult hull {};
  engfuncs.pfnTraceHull (from, to, 1, (state & rbcc::Crouched) ? head_hull : human_hull, e, &hull);
  if (hull.fAllSolid || hull.fStartSolid || hull.flFraction < 0.999f) return false;
  // A clear line over a pit is not a walkable connection.
  for (float t : {0.0f, 0.5f, 1.0f}) {
    const auto foot = from + (to - from) * t;
    TraceResult floor {};
    const auto low = foot - ystl::Vector (0.0f,0.0f,100.0f);
    engfuncs.pfnTraceLine (foot,low,1,e,&floor);
    if (floor.fAllSolid || floor.fStartSolid || floor.flFraction >= 0.99f) return false;
  }
  return true;
}
bool LearningEnabled () {
  return ready && HumanCopycat::Enabled () && cv_learning && !game.Is (GameFlags::FreeForAll);
}
LearningActor &Learner (edict_t *e, int n) {
  auto &l = learners[n];
  const int user = engfuncs.pfnGetPlayerUserId (e);
  const auto team = static_cast<uint8_t> (game.GetPlayerTeam (e));
  if (l.serial != e->serialnumber || l.user != user || l.team != team) {
    l = LearningActor {}; l.serial = e->serialnumber; l.user = user; l.team = team;
    l.random = Token (e) ^ static_cast<uint32_t> (user) ^ route_seed;
  }
  return l;
}
void CommitLearning (LearningActor &l, uint16_t next, bool terminal, uint32_t allowed = rbcc::AllLearningActions) {
  if (!l.pending) return;
  if (LearningEnabled () && cv_learning_rl && l.train && (!l.selfplay || cv_learning_selfplay))
    learning.reinforce (l.state, l.action, l.reward, next, terminal, l.selfplay, allowed);
  l.pending = false; l.reward = 0;
}
void LearningStep (Bot &bot, edict_t *target, const rbcc::Frame &frame, HumanCommand &cmd,
    const ystl::Vector &desired, bool available, bool avoid_retreat) {
  const int n = bot.Index (); auto &l = Learner (bot.Ent (), n);
  if (!LearningEnabled () || !available || frame.context.team > 1) { CommitLearning (l, 0, true); return; }
  death_seen[n] = false;
  const float now = game.Time ();
  const bool selfplay = target && ((target->v.flags & FL_FAKECLIENT) || bots[target]);
  const bool train = !selfplay || cv_learning_selfplay.As<bool> ();
  team_experience[frame.context.team] = team_experience[frame.context.team] || train;
  if (cv_learning_adaptation && target && now >= l.next_observation) {
    l.next_observation = now + 0.5f;
    const auto delta = bot.pev->origin - target->v.origin;
    const float length = delta.length ();
    const float approach = length > 1.0f ? (target->v.velocity | delta) / (length * std::max (target->v.maxspeed, 100.0f)) : 0.0f;
    learning.observeOpponent (frame.context.team, approach, target->v.velocity.length2d () < 35.0f);
  }
  const auto state = learning.state (frame, cv_learning_adaptation.As<bool> ());
  uint32_t allowed = (1u << static_cast<unsigned> (rbcc::LearnedAction::Native)) | (1u << static_cast<unsigned> (rbcc::LearnedAction::Hold));
  auto heading = desired; heading.x = heading.z = 0.0f;
  const bool decision_due = !l.pending || now >= l.next || l.target != frame.context.targetToken;
  // Revalidate each candidate against current hull geometry AND a supporting floor.
  for (unsigned a = 2; a < rbcc::LearningActions; ++a) {
    if (!decision_due && a != static_cast<unsigned> (l.action)) continue;
    if (a == 4 && avoid_retreat) continue;
    const auto direction = a == 4 ? -heading.forward () : heading.right () * (a == 2 ? -1.0f : 1.0f);
    if (HumanSegment (bot.Ent (), frame.origin, ToCore (bot.pev->origin + direction * 72.0f), frame.state)) allowed |= 1u << a;
  }
  if (l.pending && (now >= l.next || l.target != frame.context.targetToken || !(allowed & (1u << static_cast<unsigned> (l.action)))))
    CommitLearning (l, state, l.target != frame.context.targetToken, allowed);
  if (!l.pending) {
    l.state = state; l.team = frame.context.team; l.selfplay = selfplay; l.train = train;
    l.action = learning.choose (state, allowed, l.random, cv_learning_exploration.As<float> () * 0.01f,
      cv_learning_imitation.As<bool> (), cv_learning_adaptation.As<bool> ());
    l.target = frame.context.targetToken; l.started = now; l.next = now + 0.75f; l.reward = 0; l.pending = true;
  }
  if (l.action == rbcc::LearnedAction::Native) return;
  ystl::Vector motion {};
  const float speed = std::min (bot.pev->maxspeed, 250.0f);
  if (l.action == rbcc::LearnedAction::Left) motion = heading.right () * (-speed * 0.65f);
  if (l.action == rbcc::LearnedAction::Right) motion = heading.right () * (speed * 0.65f);
  if (l.action == rbcc::LearnedAction::Retreat) motion = heading.forward () * (-speed * 0.55f);
  auto view = cmd.angles; view.x = view.z = 0.0f;
  cmd.forward = motion | view.forward (); cmd.side = motion | view.right (); cmd.up = 0.0f;
  cmd.buttons &= static_cast<uint16_t> (~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
}
void EnsureSplit (Team team) {
  if (team != Team::CT && team != Team::Terrorist) return;
  auto &plan = split_teams[static_cast<size_t> (team)];
  std::vector<rbcc::SplitMember> members;
  uint32_t roster = 2166136261u;
  ystl::Vector spawn {};
  for (auto &b : bots) {
    if (!Alive (b.Ent ()) || game.GetPlayerTeam (b.Ent ()) != team) continue;
    const int index = b.Index ();
    if (index < 0 || index >= kGameMaxPlayers) continue;
    const bool bomb = (static_cast<uint32_t> (b.pev->weapons) & (1u << 6u)) != 0;
    members.push_back ({index,assignments[index].previous,bomb});
    roster = rbcc::hashBytes (&index,sizeof index,roster); roster = rbcc::hashBytes (&bomb,sizeof bomb,roster); spawn += b.pev->origin;
  }
  if (members.size () < 2 || (plan.ready && plan.roster == roster)) return;
  const auto &goals = graph.GetPoints (PointType::Goal);
  int wing[2] {kInvalidNodeIndex,kInvalidNodeIndex}; float farthest = 512.0f * 512.0f;
  for (const int a : goals) for (const int b : goals) {
    if (!graph.Exists (a) || !graph.Exists (b) || mode_walls.IsNodeBlocked (graph[a].origin) || mode_walls.IsNodeBlocked (graph[b].origin)) continue;
    const float d = graph[a].origin.distance_sq (graph[b].origin);
    if (d > farthest) { farthest = d; wing[0] = a; wing[1] = b; }
  }
  if (!graph.Exists (wing[0]) || !graph.Exists (wing[1])) return;
  spawn *= 1.0f / static_cast<float> (members.size ());
  const auto site_center = (graph[wing[0]].origin + graph[wing[1]].origin) * 0.5f;
  const auto center = (site_center + spawn) * 0.5f; // approach between spawn and the sites, not enemy spawn
  int middle = kInvalidNodeIndex; float mid_cost = 1e30f;
  for (int i = 0; i < graph.Length (); ++i) {
    const auto &node = graph[i];
    if (mode_walls.IsNodeBlocked (node.origin)) continue;
    if ((team == Team::Terrorist && has_flag (node.flags,NodeFlag::CTOnly)) ||
        (team == Team::CT && has_flag (node.flags,NodeFlag::TerroristOnly))) continue;
    if (has_flag (node.flags,NodeFlag::Goal | NodeFlag::Ladder) || node.origin.distance_sq (spawn) < 160.0f * 160.0f) continue;
    if (node.origin.distance_sq (graph[wing[0]].origin) < 300.0f * 300.0f || node.origin.distance_sq (graph[wing[1]].origin) < 300.0f * 300.0f) continue;
    unsigned links = 0; for (const auto &l : node.links) if (graph.Exists (l.index)) ++links;
    const float cost = node.origin.distance (center) + node.origin.distance (spawn) * 0.2f - static_cast<float> (links) * 35.0f;
    if (cost < mid_cost) { middle = i; mid_cost = cost; }
  }
  auto roles = rbcc::splitRoles (members,route_seed ^ roster);
  if (LearningEnabled () && cv_learning_multiagent) {
    uint32_t seed = route_seed ^ roster;
    const auto tactic = learning.chooseTactic (static_cast<unsigned> (team), seed, cv_learning_exploration.As<float> () * 0.01f);
    if (tactic != rbcc::TeamTactic::Split) for (auto &role : roles) if (role == 2)
      role = tactic == rbcc::TeamTactic::PressureA ? 0 : 1;
  }
  for (size_t i = 0; i < members.size (); ++i) {
    auto &a = assignments[members[i].slot]; a.role = roles[i]; a.middle = middle; a.middle_done = false;
    const int site = a.role == 2 ? static_cast<int> ((route_seed ^ static_cast<uint32_t> (members[i].slot)) & 1u) : a.role;
    a.goal = wing[site];
    // Several goal nodes at the same site provide varied final approaches.
    float best = 1e30f;
    for (const int g : goals) {
      if (!graph.Exists (g) || graph[g].origin.distance_sq (graph[wing[site]].origin) > 420.0f * 420.0f) continue;
      const float cost = HumanCopycat::RouteWeight (team,g);
      if (cost < best) {best = cost; a.goal = g;}
    }
  }
  plan.ready = true; plan.roster = roster;
}
void Feedback (Actor &s, bool success, bool learn = true) {
  if (!s.replay.active ()) return;
  if (learn) { database.feedback (s.replay.id (), success); dirty = true; }
  if (success) ++completed; else ++aborted;
  s.exclude = s.replay.id (); s.replay.stop (); s.frame_elapsed = 0;
  s.next_match = game.Time () + (success ? 0.5f : 1.0f);
}
uint8_t ProjectileWeapon (edict_t *e) {
  const char *m = e->v.model.chars ();
  if (std::strstr (m,"w_hegrenade.mdl")) return 4;
  if (std::strstr (m,"w_flashbang.mdl")) return 25;
  if (std::strstr (m,"w_smokegrenade.mdl")) return 9;
  return 0;
}
void ScanProjectiles () {
  // Only a real engine detonation qualifies as objective cover. A resting
  // grenade may not have bloomed yet, even though perception is conservative.
  for (const auto &g : game_state.GetActiveGrenades ()) {
    if (g.kind != GrenadeKind::Smoke || g.origin.empty () || !g.ent || g.ent->free)
      continue;
    Cloud *found = nullptr;
    for (auto &c : clouds)
      if (c.entity == g.ent && c.serial == g.ent->serialnumber && c.expires > game.Time ()) { found = &c; break; }
    if (!found)
      for (auto &c : clouds)
        if (c.expires <= game.Time ()) { found = &c; break; }
    if (found && (!found->detonated || found->expires <= game.Time ()))
      *found = {g.origin, game.Time () + 18.0f, g.ent, g.ent->serialnumber, true};
  }
  for (auto &p : projectiles) {
    if (!p.entity) continue;
    const bool gone = p.entity->free || p.entity->serialnumber != p.serial || ProjectileWeapon (p.entity) != p.weapon;
    if (!gone) p.last = p.entity->v.origin;
    const bool settled = !gone && p.weapon == 9 && p.entity->v.velocity.length_sq () < 64.0f;
    if (gone || settled) {
      if (p.owner >= 0 && p.owner < kGameMaxPlayers) recorders[p.owner].noteLanding (p.weapon, ToCore (p.last));
      p = Projectile {};
    }
  }
  unsigned checked = 0;
  game.SearchEntities ("classname", "grenade", [&] (edict_t *e) {
    if (++checked > 128) return EntitySearchResult::Break;
    const auto weapon = ProjectileWeapon (e);
    const bool settled = weapon == 9 && e->v.velocity.length_sq () < 64.0f && (e->v.flags & FL_ONGROUND);
    if (settled) {
      bool known = false;
      for (const auto &c : clouds) if (c.entity == e && c.serial == e->serialnumber) { known = true; break; }
      if (!known) for (auto &c : clouds) if (c.expires <= game.Time ()) {
        c = {e->v.origin, game.Time () + 18.0f, e, e->serialnumber}; break;
      }
    }
    const int owner = Slot (e->v.owner);
    if (!weapon || owner < 0 || !e->v.owner || (e->v.owner->v.flags & FL_FAKECLIENT) || settled) return EntitySearchResult::Continue;
    for (const auto &p : projectiles) if (p.entity == e && p.serial == e->serialnumber) return EntitySearchResult::Continue;
    for (auto &p : projectiles) if (!p.entity) { p = {e, e->serialnumber, owner, weapon, e->v.origin}; break; }
    return EntitySearchResult::Continue;
  });
}
bool UtilitySafe (edict_t *e, const rbcc::Clip &clip) {
  for (const auto &t : clip.throws) {
    if (!t.observed) return false;
    if (t.weapon == 9) continue;
    for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i) {
      auto mate = game.PlayerOfIndex (i);
      if (!Alive (mate) || game.GetPlayerTeam (mate) != game.GetPlayerTeam (e)) continue;
      const auto landing = ToEngine (t.landing);
      const float distance = mate->v.origin.distance (landing);
      if (distance < (t.weapon == 4 ? 240.0f : 650.0f) && ClearLine (mate, landing)) return false;
    }
  }
  return true;
}
rbcc::SkillModel Model () {
  auto m = database.model (Profile ());
  const float skill = cv_human_aim_skill.As<float> () * 0.01f;
  const float reaction = cv_human_reaction_skill.As<float> () * 0.01f;
  m.turnRate = Clamp (m.turnRate * (0.65f + skill * 0.5f), 60.0f, 480.0f);
  m.aimError = Clamp (m.aimError * (1.7f - skill), 0.25f, 5.0f);
  m.reactionMs = Clamp (m.reactionMs * (1.65f - reaction), 140.0f, 900.0f);
  for (unsigned i = 0; i < 31; ++i) {
    m.recoil[i] = Clamp (m.recoilSamples[i] ? m.recoil[i] : 0.72f, 0.25f, 0.92f);
    if (m.recoilSamples[i] == 0) m.recoilSamples[i] = 1;
  }
  return m;
}
void Reproject (HumanCommand &cmd, const ystl::Vector &new_angles, bool spatial) {
  auto from = cmd.angles; auto to = new_angles;
  if (!spatial) { from.x = 0.0f; to.x = 0.0f; }
  ystl::Vector f, r, u, nf, nr, nu;
  from.angle_vectors (&f, &r, &u); to.angle_vectors (&nf, &nr, &nu);
  const auto world = f * cmd.forward + r * cmd.side + u * cmd.up;
  cmd.forward = world | nf; cmd.side = world | nr; cmd.up = spatial ? world | nu : cmd.up;
  cmd.angles = new_angles;
}
} // namespace

bool HumanCopycat::Enabled () { return cv_human_copycat.ptr && cv_human_copycat.As<bool> (); }

bool HumanCopycat::SmokeCover (const ystl::Vector &point) {
  for (const auto &c : clouds)
    if (c.detonated && c.expires > game.Time () && point.distance_sq (c.origin + ystl::Vector (0,0,24)) < 150.0f * 150.0f)
      return true;
  for (const auto &g : game_state.GetActiveGrenades ())
    if (g.kind == GrenadeKind::Smoke && !g.origin.empty () && point.distance_sq (g.origin + ystl::Vector (0,0,24)) < 150.0f * 150.0f)
      return true;
  return false;
}
bool HumanCopycat::VisualPoint (edict_t *observer, edict_t *target, ystl::Vector &point, bool headFirst) {
  point.clear ();
  if (!Alive (observer) || !Alive (target) || observer == target) return false;
  const int slot = Slot (observer);
  if (slot >= 0 && actors[slot].blind_until > game.Time ()) return false;
  if (auto bot = bots[observer]; bot && !bot->blind_timer_.elapsed ()) return false;
  if ((target->v.effects & EF_NODRAW) || target->v.renderfx == kRenderFxExplode ||
      (target->v.rendermode != kRenderNormal && target->v.renderamt < 30.0f)) return false;
  const auto eyes = observer->v.origin + observer->v.view_ofs;
  const float fov = observer->v.fov > 1.0f ? observer->v.fov : 90.0f;
  const bool duck = (target->v.flags & FL_DUCKING) != 0;
  // Torso first. Head is considered only when the torso is actually occluded.
  float offsets[] = {duck ? 4.0f : 10.0f, duck ? 12.0f : 25.0f, duck ? -12.0f : -24.0f};
  if (headFirst) std::swap (offsets[0],offsets[1]);
  for (auto z : offsets) {
    const auto candidate = target->v.origin + ystl::Vector (0.0f, 0.0f, z);
    const auto a = (candidate - eyes).angles ();
    if (std::fabs (rbcc::angleDelta (a.y, observer->v.v_angle.y)) > fov * 0.5f ||
        std::fabs (rbcc::angleDelta (-a.x, observer->v.v_angle.x)) > fov * 0.42f) continue;
    if (SmokeBlocks (eyes, candidate) || !ClearLine (observer, candidate, target)) continue;
    point = candidate; return true;
  }
  return false;
}

bool HumanCopycat::HasWitness (Team team, edict_t *target) {
  for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i) {
    auto e = game.PlayerOfIndex (i); ystl::Vector point {};
    if (Alive (e) && game.GetPlayerTeam (e) == team && VisualPoint (e, target, point)) return true;
  }
  return false;
}

void HumanCopycat::LevelStart () {
  HumanChat::Reset ();
  LevelStop ();
  next_projectiles = 0.0f;
  map_name = game.GetMapName ();
  // Map names originate in engine state; disallow traversal in storage paths.
  if (map_name.empty () || map_name.size () > 63 || map_name.find_first_not_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos) return;
  bsp_hash = rbcc::hashBytes (map_name.data (), map_name.size ());
  const std::string file = "maps/" + map_name + ".bsp";
  int length = 0;
  auto bytes = engfuncs.pfnLoadFileForMe (const_cast<char *> (file.c_str ()), &length);
  if (bytes) {
    if (length > 0) bsp_hash = rbcc::hashBytes (bytes, static_cast<size_t> (length), bsp_hash);
    engfuncs.pfnFreeFile (bytes);
  }
  ready = true; LoadEnvironment ();
  round_number = 0; route_seed = 1;
  for (auto &a : assignments) a = Assignment {};
  for (auto &t : split_teams) t = TeamSplit {};
  std::memset (native_visits,0,sizeof native_visits);
  std::fill (std::begin (last_native_node),std::end (last_native_node),-1);
  NewRound ();
  if (!commands_registered) {
    game.RegisterEngineCommand ("aibo_human_status", &HumanCopycat::Status);
    game.RegisterEngineCommand ("aibo_human_save", &HumanCopycat::Save);
    game.RegisterEngineCommand ("aibo_human_reload", &HumanCopycat::Reload);
    game.RegisterEngineCommand ("aibo_learning_status", &HumanCopycat::LearningStatus);
    game.RegisterEngineCommand ("aibo_explore_status", &HumanExplore::Status);
    game.RegisterEngineCommand ("aibo_chat_status", &HumanChat::Status);
    game.RegisterEngineCommand ("aibo_final_status", &HumanArena::Status);
    game.RegisterEngineCommand ("aibo_human_aim_status", &HumanAimCoach::Status);
    commands_registered = true;
  }
  game.Print ("[Human Copycat 4.3.3 FINAL] AiBo navigation/tactics + final human input controller. clips=%u", static_cast<unsigned> (database.size ()));
}
void HumanCopycat::LevelStop () {
  for (auto &l : learners) CommitLearning (l, 0, true);
  Save (); HumanAimCoach::Stop (); HumanArena::Stop (); HumanExplore::Stop (); ready = false; ResetActors (); database.clear (); human_routes.clear (); learning.clear ();
}
void HumanCopycat::Save () {
  if (!ready || (!dirty && !human_routes.dirty () && !(learning.dirty () && cv_learning_lifelong) && !HumanExplore::Dirty () && !HumanArena::Dirty () && !HumanAimCoach::Dirty ())) return;
  HumanExplore::Save ();
  HumanArena::Save ();
  HumanAimCoach::Save ();
  std::error_code ec;
  std::filesystem::create_directories (std::filesystem::path (data_path).parent_path (), ec);
  if (ec) { game.Print ("[Human Copycat] Save directory unavailable: %s",ec.message ().c_str ()); return; }
  std::string error;
  if (!ec && human_routes.dirty () && !human_routes.save (navigation_path,map_name,environment,error)) {
    game.Print ("[Human Copycat] Human navigation save failed: %s",error.c_str ());
  }
  if (cv_learning_lifelong && learning.dirty () && !learning.save (learning_path, map_name, environment, error))
    game.Print ("[Learning] Save failed: %s", error.c_str ());
  if (!dirty) return;
  if (!ec && database.save (data_path, map_name, environment, error)) dirty = false;
  else game.Print ("[Human Copycat] Save failed; previous data retained: %s", ec ? ec.message ().c_str () : error.c_str ());
}
void HumanCopycat::Reload () {
  if (ready) {
    for (auto &l : learners) CommitLearning (l, 0, true);
    Save (); if (!dirty && !human_routes.dirty () && !(cv_learning_lifelong && learning.dirty ()) && !HumanExplore::Dirty () && !HumanArena::Dirty () && !HumanAimCoach::Dirty ()) LoadEnvironment ();
  }
}
void HumanCopycat::Frame () {
  if (!ready) return;
  HumanExplore::Frame ();
  if (game.Time () >= next_projectiles) { ScanProjectiles (); next_projectiles = game.Time () + 0.1f; }
  if (game.Time () >= next_environment) {
    next_environment = game.Time () + 1.0f;
    if (Fingerprint () != environment) {
      for (auto &l : learners) CommitLearning (l, 0, true);
      Save (); if (!dirty && !human_routes.dirty () && !(cv_learning_lifelong && learning.dirty ()) && !HumanExplore::Dirty () && !HumanArena::Dirty () && !HumanAimCoach::Dirty ()) LoadEnvironment ();
    }
  }
  if (game.Time () >= next_save) { Save (); next_save = game.Time () + cv_human_autosave.As<float> (); }
}
void HumanCopycat::Record (const edict_t *player, const usercmd_t *cmd) {
  const int n = Slot (player);
  if (n < 0 || !cmd) return;
  auto e = const_cast<edict_t *> (player);
  // CmdStart is re-entrant through RunPlayerMove. Reject bots BEFORE touching
  // their shared slot state, otherwise every command would reset HumanAim.
  if ((e->v.flags & FL_FAKECLIENT) || bots[e]) { recorders[n].reset (); human_routes.resetTeacher (n); return; }
  const int user_id = engfuncs.pfnGetPlayerUserId (e);
  if (actors[n].serial != e->serialnumber || actors[n].user_id != user_id) {
    const float blind_until = actors[n].blind_until;
    actors[n] = Actor {}; actors[n].blind_until = blind_until; actors[n].serial = e->serialnumber; actors[n].user_id = user_id; recorders[n].reset (); human_routes.resetTeacher (n);
  }
  if (!ready || !Enabled () || (e->v.flags & FL_FAKECLIENT) || bots[e] || !Alive (e) ||
      game_state.IsRoundOver () || e->v.maxspeed < 10.0f || e->v.movetype == MOVETYPE_NOCLIP) {
    recorders[n].reset (); human_routes.resetTeacher (n); return;
  }
  auto frame = Snapshot (e,true,HumanAimCoach::Enabled () ? 8192.0f : 2048.0f);
  frame.angles = ToCore (cmd->viewangles); frame.forward = cmd->forwardmove; frame.side = cmd->sidemove;
  frame.up = cmd->upmove; frame.buttons = cmd->buttons; frame.impulse = cmd->impulse; frame.msec = cmd->msec;
  death_seen[n] = false;
  // Aim features are relative to this exact user command, never a later snapshot.
  if (frame.context.visible) {
    const auto a = (ToEngine (frame.context.enemy) - (e->v.origin + e->v.view_ofs)).angles ();
    frame.context.enemyYaw = rbcc::angleDelta (a.y, cmd->viewangles.y);
    frame.context.enemyPitch = rbcc::angleDelta (-a.x, cmd->viewangles.x);
  }
  HumanAimCoach::RecordHuman (e,frame);
  if (!cv_human_record || (cv_human_teacher.As<int> () > 0 && cv_human_teacher.As<int> () != n + 1)) {recorders[n].reset ();human_routes.resetTeacher (n);return;}
  if (LearningEnabled () && cv_learning_imitation && frame.context.visible && frame.context.team < 2 &&
      (frame.state & rbcc::Ground) && !(frame.state & (rbcc::Ladder | rbcc::Water)) &&
      !(frame.buttons & (rbcc::Use | rbcc::Jump)) && !rbcc::isUtility (frame.weapon) &&
      frame.weapon != 6 && frame.weapon != 29 && std::fabs (frame.context.enemyYaw) < 40.0f) {
    auto &l = Learner (e, n);
    if (game.Time () >= l.next_imitation) {
      l.next_imitation = game.Time () + 0.25f;
      learning.imitate (learning.state (frame, cv_learning_adaptation.As<bool> ()), rbcc::OnlineLearning::classify (frame));
    }
  }
  const char *auth = engfuncs.pfnGetPlayerAuthId (e);
  if (!auth || !*auth || std::strstr (auth,"LAN") || std::strstr (auth,"PENDING") || std::strstr (auth,"UNKNOWN")) auth = e->v.netname.chars ();
  uint32_t profile = rbcc::hashBytes (auth, std::strlen (auth));
  if (cv_human_waypoints) {
    if (frame.velocity.length () < 25.0f && game.Time () >= actors[n].next_cover_check) {
      actors[n].next_cover_check = game.Time () + 0.5f; actors[n].observed_cover = false;
      const auto eye = e->v.origin + e->v.view_ofs;
      for (const auto &direction : {ystl::Vector (96.0f,0.0f,0.0f),ystl::Vector (-96.0f,0.0f,0.0f),
          ystl::Vector (0.0f,96.0f,0.0f),ystl::Vector (0.0f,-96.0f,0.0f)}) {
        TraceResult cover {}; const auto end = eye + direction;
        engfuncs.pfnTraceLine (eye,end,1,e,&cover);
        if (cover.flFraction < 0.75f) actors[n].observed_cover = true;
      }
    }
    const bool hiding = game.Time () - game_state.GetRoundStartTime () > 8.0f &&
      (actors[n].observed_cover || (frame.state & rbcc::Crouched));
    human_routes.observe (frame,n,hiding);
  }
  if (recorders[n].capture (frame, profile ? profile : 1u, database)) { dirty = true; ++recorded; }
}
void HumanCopycat::Blind (edict_t *e, float seconds) {
  const int n = Slot (e);
  if (n >= 0) {
    if (actors[n].serial != e->serialnumber) {actors[n] = Actor {};actors[n].serial=e->serialnumber;}
    actors[n].blind_until = game.Time () + Clamp (seconds, 0.0f, 20.0f);
  }
}
void HumanCopycat::ResetBot (Bot &bot) {
  const int n = bot.Index (); if (n < 0 || n >= kGameMaxPlayers) return;
  Feedback (actors[n], false, false); actors[n] = Actor {};
  // Respawns, removals and team switches do not manufacture a failed action.
  learners[n] = LearningActor {}; death_seen[n] = false;
  HumanExplore::Reset (n);
  HumanArena::ResetBot (bot);
  HumanAimCoach::ResetBot (bot);
  bot.pending_generated_chat_.clear (); bot.next_generated_chat_ = 0.0f;
}
void HumanCopycat::BeforeThink (Bot &bot) {
  if (!Enabled ()) return;
  HumanArena::BeforeThink (bot);
  const int index = bot.Index ();
  if (index >= 0 && index < kGameMaxPlayers && graph.Exists (bot.current_node_index_) &&
      last_native_node[index] != bot.current_node_index_ && (bot.team_ == Team::CT || bot.team_ == Team::Terrorist)) {
    auto &used = native_visits[static_cast<size_t> (bot.team_)][static_cast<size_t> (bot.current_node_index_)];
    if (used < 1000) ++used;
    last_native_node[index] = bot.current_node_index_;
  }
  if (NavigatingHuman (bot) || HumanExplore::Active (bot)) bot.IgnoreCollision ();
  if (bot.GetTaskId () == TaskId::Normal) {
    int goal = HumanArena::Goal (bot);
    if (!graph.Exists (goal)) goal = StrategicGoal (bot);
    if (graph.Exists (goal) && goal != bot.Task ()->data && game.IsNullEntity (bot.enemy_)) {
      bot.ClearSearchNodes (); bot.Task ()->data = goal; bot.chosen_goal_index_ = goal;
    }
  }
  ystl::Vector point {};
  if (!game.IsNullEntity (bot.enemy_) && !VisualPoint (bot.Ent (), bot.enemy_, point)) {
    bot.enemy_ = nullptr; bot.enemy_origin_.clear (); bot.enemy_parts_ = Visibility::None;
    bot.states_ &= ~Sense::SeeingEnemy; bot.aim_flags_ &= ~AimFlags::Enemy; bot.wants_to_fire_ = false;
  }
}
void HumanCopycat::NewRound () {
  if (!Enabled ()) return;
  for (auto &l : learners) { CommitLearning (l, 0, true); l = LearningActor {}; }
  std::fill (std::begin (death_seen), std::end (death_seen), false);
  learning_round_finished = false; team_experience[0] = team_experience[1] = false;
  if (LearningEnabled ()) learning.newRound ();
  ++round_number;
  route_seed = static_cast<uint32_t> (ystl::rg (1,0x7fffffff)) ^ (round_number * 2654435761u);
  for (auto &a : assignments) { a.previous = a.role; a.role = a.goal = a.middle = -1; a.middle_done = false; }
  for (auto &p : split_teams) p = TeamSplit {};
  for (auto &team : native_visits) for (auto &n : team) n = static_cast<uint16_t> (n * 3u / 4u);
  std::fill (std::begin (last_native_node),std::end (last_native_node),-1);
  human_routes.newRound ();
  HumanExplore::NewRound ();
  HumanChat::NewRound ();
  HumanArena::NewRound ();
}
bool HumanCopycat::SpreadActive () {
  return Enabled () && cv_human_spread.ptr && cv_human_spread.As<bool> () && game.MapIs (MapFlags::Demolition) &&
    !game_state.IsBombPlanted () && !game_state.IsRoundOver () &&
    game.Time () - game_state.GetRoundStartTime () < cv_human_spread_seconds.As<float> ();
}
int HumanCopycat::StrategicGoal (Bot &bot) {
  if (HumanExplore::Scout (bot)) return kInvalidNodeIndex;
  if (!SpreadActive () || bot.is_vip_ || bot.has_hostage_ || cv_debug_goal.As<int> () >= 0 ||
      !Alive (bot.Ent ()) || bot.HasBombTask ()) return kInvalidNodeIndex;
  EnsureSplit (game.GetPlayerTeam (bot.Ent ()));
  const int n = bot.Index (); if (n < 0 || n >= kGameMaxPlayers) return kInvalidNodeIndex;
  auto &a = assignments[n];
  if (a.role < 0 || !graph.Exists (a.goal)) return kInvalidNodeIndex;
  if (a.role == 2 && graph.Exists (a.middle) && !a.middle_done) {
    if (bot.pev->origin.distance_sq (graph[a.middle].origin) > 110.0f * 110.0f &&
        game.Time () - game_state.GetRoundStartTime () < 35.0f) return a.middle;
    a.middle_done = true;
  }
  return a.goal;
}
bool HumanCopycat::MayAssist (const Bot &bot, const ystl::Vector &origin) {
  if (!SpreadActive ()) return true;
  // At most two nearby responders abandon their wing for one contact.
  unsigned nearer = 0; const float own = bot.pev->origin.distance_sq (origin);
  for (const auto &mate : bots) if (&mate != &bot && Alive (mate.Ent ()) && game.GetPlayerTeam (mate.Ent ()) == bot.team_ &&
      (mate.pev->origin.distance_sq (origin) + 1.0f < own ||
       (std::fabs (mate.pev->origin.distance_sq (origin) - own) < 1.0f && mate.Index () < bot.Index ()))) ++nearer;
  return nearer < 2;
}
float HumanCopycat::RouteWeight (Team team, int node) {
  if (!Enabled () || !cv_human_route_variety || node < 0 || node >= 4096 || (team != Team::CT && team != Team::Terrorist)) return 1.0f;
  uint32_t h = route_seed ^ static_cast<uint32_t> (node) * 2246822519u ^ static_cast<uint32_t> (team) * 3266489917u;
  h = rbcc::hashBytes (&h,sizeof h);
  return 0.8f + static_cast<float> (h & 65535u) * (0.55f / 65535.0f) +
    std::min (static_cast<float> (native_visits[static_cast<size_t> (team)][static_cast<size_t> (node)]) * 0.12f,1.2f);
}
bool HumanCopycat::NavigatingHuman (const Bot &bot) {
  const int n = bot.Index ();
  return Enabled () && cv_human_waypoints && n >= 0 && n < kGameMaxPlayers && actors[n].route_active;
}
bool HumanCopycat::Navigate (Bot &bot, HumanCommand &cmd, ystl::Vector &desired) {
  const int index = bot.Index (); if (index < 0 || index >= kGameMaxPlayers) return false;
  auto &s = actors[index];
  if (!cv_human_waypoints || bot.GetTaskId () != TaskId::Normal || game_state.IsBombPlanted () ||
      bot.has_hostage_ || bot.is_vip_ || human_routes.nodes ().empty ()) {s.route_active = false; return false;}
  const auto team = static_cast<uint8_t> (bot.team_); if (team > 1) return false;
  auto stop = [&] () {
    s.route_active = false; s.route.clear (); s.hold_until = 0.0f;
    s.next_route = game.Time () + 3.0f + static_cast<float> (index % 5) * 0.17f;
    bot.ClearSearchNodes (); bot.current_node_index_ = kInvalidNodeIndex;
  };
  if (!s.route_active && game.Time () >= s.next_route) {
    s.next_route = game.Time () + 4.0f + static_cast<float> (index % 7) * 0.19f;
    int goal = bot.Task ()->data;
    int split = HumanArena::Goal (bot);
    if (!graph.Exists (split)) split = StrategicGoal (bot);
    if (graph.Exists (split)) goal = split;
    if (!graph.Exists (goal) || bot.pev->origin.distance_sq (graph[goal].origin) < 120.0f * 120.0f) return false;
    auto target = graph[goal].origin; s.hide_node = -1;
    const uint32_t seed = route_seed ^ static_cast<uint32_t> (index + 1) * 2654435761u;
    if (cv_human_hide && !bot.has_c4_ && (bot.team_ == Team::CT || bot.fear_level_ > 0.5f) && (seed % 4u == 0u)) {
      const int hide = human_routes.favoriteHide (ToCore (target),team,seed);
      if (hide >= 0 && HumanArena::Fresh (bot,ToEngine (human_routes.nodes ()[static_cast<size_t> (hide)].origin)-ystl::Vector(0,0,36)) && bot.NumFriendsNear (ToEngine (human_routes.nodes ()[static_cast<size_t> (hide)].origin),96.0f) == 0) {
        s.hide_node = hide; target = ToEngine (human_routes.nodes ()[static_cast<size_t> (hide)].origin);
      }
    }
    const auto began = std::chrono::steady_clock::now (); unsigned checks = 0;
    auto validator = [&] (const rbcc::Vec &a,const rbcc::Vec &b,uint8_t flags) {
      if (++checks > 192 || std::chrono::steady_clock::now () - began > std::chrono::milliseconds (route_budget_ms)) return false;
      return HumanSegment (bot.Ent (),a,b,flags);
    };
    s.route = human_routes.plan (ToCore (bot.pev->origin),ToCore (target),team,seed,validator);
    if (s.route.size () < 2) return false;
    s.route_cursor = 0; s.route_active = true; s.route_progress = game.Time (); s.route_distance = 1e30f;
  }
  if (!s.route_active) return false;
  if (s.hold_until > game.Time () && s.hide_node >= 0) {
    const auto &hide = human_routes.nodes ()[static_cast<size_t> (s.hide_node)];
    cmd.forward = cmd.side = cmd.up = 0.0f; cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2 | IN_JUMP));
    if (hide.state & rbcc::Crouched) cmd.buttons |= IN_DUCK;
    desired = ToEngine (hide.look); return true;
  }
  while (s.route_cursor < s.route.size () && bot.pev->origin.distance_sq (ToEngine (human_routes.nodes ()[s.route[s.route_cursor]].origin)) < 28.0f * 28.0f) {
    human_routes.used (s.route[s.route_cursor],team); ++s.route_cursor; s.route_progress = game.Time (); s.route_distance = 1e30f;
  }
  if (s.route_cursor >= s.route.size ()) {
    if (s.hide_node >= 0 && s.hold_until <= 0.0f && bot.pev->origin.distance_sq (ToEngine (human_routes.nodes ()[static_cast<size_t> (s.hide_node)].origin)) < 64.0f * 64.0f) {
      HumanArena::Visit (bot,ToEngine (human_routes.nodes ()[static_cast<size_t> (s.hide_node)].origin)-ystl::Vector(0,0,36),2);
      s.hold_until = game.Time () + 1.5f + static_cast<float> ((route_seed ^ static_cast<uint32_t> (index)) % 35u) * 0.1f;
      cmd.forward = cmd.side = cmd.up = 0.0f; return true;
    }
    HumanArena::Visit (bot,bot.pev->origin-ystl::Vector (0,0,(bot.pev->flags & FL_DUCKING) ? 18.0f : 36.0f),0);
    stop (); return false;
  }
  const auto &node = human_routes.nodes ()[s.route[s.route_cursor]];
  const auto target = ToEngine (node.origin); const float distance = bot.pev->origin.distance (target);
  if (distance + 3.0f < s.route_distance) {s.route_distance = distance; s.route_progress = game.Time ();}
  if (game.Time () - s.route_progress > 1.25f || !HumanSegment (bot.Ent (),ToCore (bot.pev->origin),node.origin,node.state)) {stop (); return false;}
  auto heading = (target - bot.pev->origin).angles (); heading.x = 0.0f;
  cmd.angles = heading; cmd.forward = std::min (bot.pev->maxspeed,distance < 48.0f ? 140.0f : bot.pev->maxspeed);
  cmd.side = cmd.up = 0.0f; cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2 | IN_JUMP));
  if (node.state & rbcc::Crouched) cmd.buttons |= IN_DUCK;
  if (target.z - bot.pev->origin.z > 18.0f && bot.IsOnFloor ()) cmd.buttons |= IN_JUMP;
  const auto view = target + bot.pev->view_ofs;
  desired = (view - bot.GetEyesPos ()).angles (); desired.x = -desired.x;
  bot.IgnoreCollision (); return true;
}
void HumanCopycat::Finalize (Bot &bot, HumanCommand &cmd) {
  if (bot.current_weapon_ == Weapon::Knife && bot.GetTaskId () != TaskId::ShootBreakable &&
      (game.IsNullEntity (bot.enemy_) || bot.pev->origin.distance (bot.enemy_->v.origin) > 85.0f ||
       util.ViewDot (bot.enemy_, bot.pev->origin) >= -0.35f)) {
    cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2));
  }
  // Enforce the release loadout even if the human aim/replay toggle is disabled.
  // Covers weapons granted by a map or third-party plugin after spawning.
  if (bot.is_alive_ && bot.current_weapon_ != Weapon::Invalid && !human_policy::Usable (bot.current_weapon_)) {
    cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2 | IN_RELOAD));
    bot.pev->button = cmd.buttons;
    bot.reload_data_.Clear (); bot.SelectBestWeapon ();
    return;
  }
  const int n = bot.Index ();
  if (n < 0 || n >= kGameMaxPlayers) return;
  auto &s = actors[n];
  if (!Enabled ()) { s.initialized = false; s.replay.stop (); return; }
  cmd.msec = static_cast<uint8_t> (std::clamp (static_cast<int> (cmd.msec), 1, 100));
  auto e = bot.Ent ();
  if (!Alive (e) || !bot.is_alive_) {
    // Some mods suppress DeathMsg. Close the pending episode without inventing a kill.
    CommitLearning (learners[n], 0, true); s = Actor {}; return;
  }
  if (!s.initialized || s.serial != e->serialnumber || game.Time () < s.last_time) {
    const float preserve_blind = s.serial == e->serialnumber ? s.blind_until : 0.0f;
    s = Actor {}; s.blind_until = preserve_blind; s.serial = e->serialnumber; s.initialized = true;
    s.next_route = game.Time () + static_cast<float> (n) * 0.11f;
    s.aim.reset (ToCore (e->v.v_angle), static_cast<uint32_t> (n + 1) * 2654435761u ^ static_cast<uint32_t> (e->serialnumber));
  }
  s.last_time = game.Time ();

  const auto task = bot.GetTaskId ();
  const bool objective = bot.has_progress_bar_ || bot.HasBombTask () || task == TaskId::PickupItem || (cmd.buttons & IN_USE) ||
    bot.current_weapon_ == Weapon::C4;
  const bool native_utility = bot.is_using_grenade_ || has_flag (bot.aim_flags_, AimFlags::Grenade);
  const bool blocked = !bot.blind_timer_.elapsed () || e->v.maxspeed < 10.0f || game_state.IsRoundOver () || !bot.IsMoveAllowed () || cv_freeze_bots.As<bool> () || s.blind_until > game.Time ();
  auto current = Snapshot (e, false);
  current.weapon = static_cast<uint8_t> (bot.current_weapon_);
  // Target selection remains AiBo's; its visibility is confirmed on every command.
  if (!game.IsNullEntity (bot.enemy_)) current.context = ContextFor (e, bot.enemy_,true,
    HumanAimCoach::Enabled () && (HumanAimCoach::Level (bot)>0 || cv_human_aim_level.As<float> ()>0) ? 8192.0f : 2048.0f);
  const bool visible = current.context.visible != 0;
  const float expertise = HumanAimCoach::Update (bot,visible ? bot.enemy_ : nullptr);
  auto model = rbcc::expertModel (Model (),expertise);
  if (expertise > 0 && cv_human_hltv_skill) {
    const float gap = 1.0f-human_policy::RankPower (e);
    model.aimError *= 1.0f+gap*1.5f;
    model.reactionMs += gap*120.0f;
  }
  if (expertise > 0) {
    const float proficiency = HumanAimCoach::Practice (bot)*expertise;
    model.aimError *= 1.0f-proficiency*0.14f;
    model.reactionMs = std::max (140.0f,model.reactionMs*(1.0f-proficiency*0.06f));
  }
  auto desired = bot.look_at_.empty () ? e->v.v_angle : (bot.look_at_ - bot.GetEyesPos ()).angles ();
  if (!bot.look_at_.empty ()) desired.x = -desired.x;
  if (!bot.ForcedLookAt ().empty ()) {
    desired = (bot.ForcedLookAt () - bot.GetEyesPos ()).angles (); desired.x = -desired.x;
  }
  if (visible && !native_utility) {
    auto aim_point = ToEngine (current.context.enemy);
    const bool prefer_head = HumanAimCoach::PreferHead (bot,current.context.targetToken);
    if (prefer_head) VisualPoint (e,bot.enemy_,aim_point,true);
    const auto learned_point = aim_point + ystl::Vector (0.0f, 0.0f, model.aimHeight);
    if (!prefer_head && model.heightSamples > 0 && ClearLine (e, learned_point, bot.enemy_) && !SmokeBlocks (bot.GetEyesPos (), learned_point)) aim_point = learned_point;
    desired = (aim_point - bot.GetEyesPos ()).angles (); desired.x = -desired.x;
    HumanAimCoach::Seen (bot,current.context.targetToken,aim_point);
  }
  if (native_utility && bot.grenade_.length_sq () > 100.0f) {
    const auto launch = bot.grenade_.angles ();
    const float pitch = -launch.x;
    desired = {Clamp ((pitch + 10.0f) * (pitch < -10.0f ? 90.0f / 80.0f : 90.0f / 100.0f), -89.0f, 89.0f), launch.y, 0.0f};
  }
  const bool can_control = ready && !objective && !native_utility && !blocked &&
    task != TaskId::ShootBreakable && bot.buying_finished_ && !bot.IsStuckState ();
  HumanArena::IdleLook (bot,desired);
  const bool special = HumanArena::Motor (bot,cmd,desired,ready && !objective && !native_utility && !blocked && !visible);
  const bool exploring = !special && HumanExplore::Move (bot,cmd,desired,ready && !objective && !native_utility && !blocked && !visible);
  const bool can_replay = can_control && cv_human_replay && !exploring && !special && !HumanArena::VantageHolding (bot);
  auto finish_replay = [&] () {
    if (!s.replay.active () || !s.replay.finished ()) return;
    const auto &clip = s.replay.clip ();
    const bool success = rbcc::distance (current.origin,clip.end) < 96.0f;
    if (success && !visible && rbcc::distance (clip.frames.front ().origin,clip.end) > 160.0f)
      HumanArena::Visit (bot,e->v.origin-ystl::Vector (0,0,(e->v.flags & FL_DUCKING) ? 18.0f : 36.0f),0);
    Feedback (s,success);
  };
  finish_replay ();
  if (s.replay.active () && (!can_replay || current.context.targetToken != s.replay_target ||
      current.context.objective != s.replay.clip ().frames.front ().context.objective)) Feedback (s, false, false);
  if (can_replay && !s.replay.active () && game.Time () >= s.next_match) {
    s.next_match = game.Time () + 0.3f;
    rbcc::Query q; q.current = current; q.goal = ToCore (bot.dest_origin_); q.hasGoal = !bot.dest_origin_.empty ();
    q.weapons = static_cast<uint32_t> (e->v.weapons); q.profile = Profile (); q.excludeId = s.exclude;
    q.candidates = static_cast<size_t> (cv_human_candidates.As<int> ()); q.selectionSeed = static_cast<uint32_t> (n + 1) ^ static_cast<uint32_t> (commands);
    q.combat = visible; q.utility = true;
    if (const auto clip = database.match (q); clip && !(clip->features & rbcc::Interaction) && UtilitySafe (e, *clip)) {
      const bool repeated_destination = !visible && rbcc::distance (clip->frames.front ().origin,clip->end) > 160.0f &&
        !HumanArena::Fresh (bot,ToEngine (clip->end)-ystl::Vector (0,0,36));
      if (repeated_destination) s.exclude = clip->id;
      else if (s.replay.start (*clip)) { s.replay_target = current.context.targetToken; s.frame_elapsed = 0; }
    }
  }
  bool replay_utility = false;
  finish_replay ();
  if (s.replay.active ()) {
    const auto &f = s.replay.clip ().frames[s.replay.cursor ()];
    if (rbcc::distance (current.origin, f.origin) > 72.0f || current.weapon != f.weapon ||
        (current.state & (rbcc::Ladder | rbcc::Water)) != (f.state & (rbcc::Ladder | rbcc::Water)) ||
        !UtilitySafe (e, s.replay.clip ())) {
      Feedback (s, false);
    }
    else {
      // Never merge adjacent usercmds: a 10ms duck press must survive a 25ms AI tick.
      // TickManager emits the rest of its budget as subsequent final-controller slices.
      const auto remaining = static_cast<unsigned> (f.msec) - s.frame_elapsed;
      cmd.msec = static_cast<uint8_t> (std::min (static_cast<unsigned> (cmd.msec), remaining));
      cmd.angles = ToEngine (f.angles); cmd.forward = f.forward; cmd.side = f.side; cmd.up = f.up;
      cmd.buttons = static_cast<uint16_t> (f.buttons & ~IN_USE); cmd.impulse = 0;
      if (!visible) desired = ToEngine (f.angles);
      replay_utility = rbcc::isUtility (current.weapon);
      s.frame_elapsed += cmd.msec;
      s.replay.advance (static_cast<double> (cmd.msec));
      if (s.replay.due ()) { s.replay.consume (); s.frame_elapsed = 0; }
    }
  }
  const bool human_navigation = !special && !exploring && can_replay && !visible && !s.replay.active () && Navigate (bot,cmd,desired);
  if (!can_replay || visible || s.replay.active ()) { s.route_active = false; s.route.clear (); s.hold_until = 0.0f; }
  (void)human_navigation;
  const bool learning_movement = can_control && visible && cv_human_movement && !s.replay.active () && !s.route_active &&
    !learning_round_finished &&
    bot.IsOnFloor () && !bot.IsOnLadder () && !bot.IsInWater () && !bot.IsInNarrowPlace () &&
    !(cmd.buttons & (IN_JUMP | IN_USE | IN_DUCK)) && !bot.jump_knife_drawn_ &&
    current.weapon != 0 && current.weapon != 6 && current.weapon != 29 && !rbcc::isUtility (current.weapon);
  LearningStep (bot, bot.enemy_, current, cmd, desired, learning_movement, bot.has_c4_ || bot.is_vip_ || bot.has_hostage_);
  const bool aim_available = ready && !objective && !native_utility && !blocked && bot.buying_finished_ && !bot.reload_data_.is_reloading &&
    !special && task != TaskId::ShootBreakable;
  ystl::Vector remembered {}; uint32_t remembered_token = 0;
  const bool wallbang = !visible && aim_available && HumanAimCoach::Wallbang (bot,remembered,remembered_token);
  if (wallbang) {desired = (remembered-bot.GetEyesPos ()).angles ();desired.x=-desired.x;}
  HumanAimCoach::Prepare (bot,cmd,desired,visible,wallbang,aim_available,special || exploring || s.route_active || s.replay.active ());
  const float aim_distance = visible ? current.context.enemyDistance : (wallbang ? remembered.distance (bot.GetEyesPos ()) : 0.0f);
  const float dt = static_cast<float> (cmd.msec) * 0.001f;
  const float stress = s.stress.update (dt, static_cast<int> (e->v.health), visible, current.context.friends, bot.reload_data_.is_reloading);
  if (stress > 0.3f) bot.fear_level_ = std::max (bot.fear_level_, stress);
  bool request_fire = (cmd.buttons & IN_ATTACK) != 0 || (current.weapon == 29 && (cmd.buttons & IN_ATTACK2));
  if (!visible && !wallbang && !native_utility && !replay_utility && !objective && task != TaskId::ShootBreakable) request_fire = false;
  if (expertise > 0 && cv_human_aim_techniques && aim_available && (visible || wallbang)) request_fire = true;
  const auto result = s.aim.update (dt, ToCore (desired), ToCore (e->v.punchangle), current.weapon,
    visible ? current.context.targetToken : remembered_token, request_fire, stress, model, aim_distance,
    e->v.velocity.length2d (), e->v.fov < 89.0f, true, expertise, expertise > 0 && cv_human_aim_techniques);
  Reproject (cmd, ToEngine (result.angles), bot.IsOnLadder () || bot.IsInWater ());
  // Small, fallible input mechanics only. Recorded demonstrations retain their
  // own exact timings; objectives, ladders, jump links and utility take priority.
  if (!(expertise > 0 && cv_human_aim_techniques) && cv_human_movement && can_control && !special && !exploring && !s.replay.active () && !s.route_active &&
      !(learners[n].pending && learners[n].action != rbcc::LearnedAction::Native) &&
      bot.IsOnFloor () && !bot.IsOnLadder () && !bot.IsInWater () &&
      !(cmd.buttons & (IN_JUMP | IN_USE)) && !bot.IsInNarrowPlace () && !bot.jump_knife_drawn_) {
    const float now = game.Time ();
    const float speed = e->v.velocity.length2d ();
    if (visible && request_fire && speed > 85.0f && now >= s.next_brake && current.context.enemyDistance > 250.0f) {
      s.brake_until = now + bot.rg (0.06f, 0.1f); s.next_brake = now + bot.rg (0.55f, 0.95f);
    }
    if (visible && now < s.brake_until && speed > 45.0f) {
      // Counter-steer against measured momentum, never set velocity directly.
      auto yaw = cmd.angles; yaw.x = yaw.z = 0.0f;
      cmd.forward = Clamp (-(e->v.velocity | yaw.forward ()) * 1.2f, -220.0f, 220.0f);
      cmd.side = Clamp (-(e->v.velocity | yaw.right ()) * 1.2f, -220.0f, 220.0f);
      cmd.buttons &= static_cast<uint16_t> (~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
      if (cmd.forward > 1.0f) cmd.buttons |= IN_FORWARD;
      if (cmd.forward < -1.0f) cmd.buttons |= IN_BACK;
      if (cmd.side > 1.0f) cmd.buttons |= IN_MOVERIGHT;
      if (cmd.side < -1.0f) cmd.buttons |= IN_MOVELEFT;
    }
    if (now >= s.next_duck && !(cmd.buttons & IN_DUCK) && speed > 50.0f &&
        ((visible && current.context.enemyDistance > 350.0f) || (!visible && has_flag (bot.states_, Sense::HearingEnemy)))) {
      s.next_duck = now + bot.rg (2.0f, 4.0f);
      if (bot.rg.chance (visible ? 30 : 50)) {
        s.duck_start = now; s.double_duck = !visible;
      }
    }
    const float duck_time = now - s.duck_start;
    if (s.duck_start > 0.0f && duck_time < 0.22f) {
      if (duck_time < 0.06f || (s.double_duck && duck_time >= 0.14f && duck_time < 0.20f)) cmd.buttons |= IN_DUCK;
      else cmd.buttons &= static_cast<uint16_t> (~IN_DUCK);
    }
    else s.duck_start = 0.0f;
  }
  const bool firearm = current.weapon != 0 && current.weapon != 6 && current.weapon != 29 && !rbcc::isUtility (current.weapon);
  if (firearm && task != TaskId::ShootBreakable && (!result.fire || blocked)) cmd.buttons &= static_cast<uint16_t> (~IN_ATTACK);
  if (firearm && task != TaskId::ShootBreakable && !objective && !native_utility)
    HumanAimCoach::Finish (bot,cmd,visible,wallbang,aim_available,result.fire,aim_distance);
  if (firearm && task == TaskId::ShootBreakable &&
      (blocked || std::hypot (rbcc::angleDelta (desired.x, cmd.angles.x), rbcc::angleDelta (desired.y, cmd.angles.y)) > 2.0f)) {
    cmd.buttons &= static_cast<uint16_t> (~IN_ATTACK);
  }
  if (current.weapon == 29 && !objective &&
      (!visible || !result.fire || game.IsNullEntity (bot.enemy_) ||
       e->v.origin.distance (bot.enemy_->v.origin) > 85.0f || util.ViewDot (bot.enemy_, e->v.origin) >= -0.35f)) {
    cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2));
  }
  if ((native_utility || replay_utility) && rbcc::isUtility (current.weapon)) {
    // Hold until a bounded human turn reaches the intended launch direction.
    // Native trajectory correction is disabled separately: engine physics wins.
    const float error = std::hypot (rbcc::angleDelta (desired.x, cmd.angles.x), rbcc::angleDelta (desired.y, cmd.angles.y));
    if (error > 2.0f || blocked) cmd.buttons |= IN_ATTACK;
  }
  else if (native_utility && !objective) {
    // A pending grenade task can still have a rifle equipped this frame.
    cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2));
  }
  // Recheck the actual final ray for friendly fire, after human aim/recoil.
  if (firearm && (cmd.buttons & IN_ATTACK)) {
    TraceResult tr {}; const auto eyes = bot.GetEyesPos ();
    const auto end = eyes + (cmd.angles + e->v.punchangle).forward () * 8192.0f;
    engfuncs.pfnTraceLine (eyes, end, 0, e, &tr);
    if (Alive (tr.pHit) && game.GetPlayerTeam (tr.pHit) == bot.team_ && !game.Is (GameFlags::FreeForAll)) cmd.buttons &= static_cast<uint16_t> (~IN_ATTACK);
  }
  if (!std::isfinite (cmd.forward) || !std::isfinite (cmd.side) || !std::isfinite (cmd.up) ||
      !std::isfinite (cmd.angles.x) || !std::isfinite (cmd.angles.y)) {
    cmd.angles = e->v.v_angle; cmd.forward = cmd.side = cmd.up = 0.0f; cmd.buttons = 0;
    s.initialized = false;
  }
  cmd.angles.clamp_angles (); cmd.angles.x = Clamp (cmd.angles.x, -89.0f, 89.0f); cmd.angles.z = 0.0f;
  e->v.v_angle = cmd.angles; e->v.button = cmd.buttons;
  bot.UpdateBodyAngles (); ++commands;
}
unsigned HumanCopycat::RoundNumber () { return round_number; }
void HumanCopycat::Status () {
  const auto &m = database.model (Profile ());
  game.Print ("[Human Copycat 4.3.3 FINAL] loadout=AK47/M4A1,AWP,Deagle,USP,Glock18 roster_lock=%d chat=%d generated=%d awp_share=%d%%",
    cv_human_roster_lock.As<int> (), cv_chat.As<int> (), cv_human_generative_chat.As<int> (), cv_human_awp_percent.As<int> ());
  game.Print ("[Human Copycat] human nodes=%u edges=%u round=%u spread=%d",
    static_cast<unsigned> (human_routes.nodes ().size ()),static_cast<unsigned> (human_routes.edgeCount ()),round_number,SpreadActive () ? 1 : 0);
  game.Print ("[Human Copycat 4.3.3 FINAL] enabled=%d clips=%u frames=%u recorded=%llu commands=%llu completed=%llu aborted=%llu",
    Enabled () ? 1 : 0, static_cast<unsigned> (database.size ()), static_cast<unsigned> (database.frameCount ()),
    static_cast<unsigned long long> (recorded), static_cast<unsigned long long> (commands),
    static_cast<unsigned long long> (completed), static_cast<unsigned long long> (aborted));
  game.Print ("[Human Copycat] profile=%u reaction=%.0fms turn=%.0fdeg/s error=%.2fdeg path=%s", Profile (),
    static_cast<double> (m.reactionMs), static_cast<double> (m.turnRate), static_cast<double> (m.aimError), data_path.c_str ());
  LearningStatus ();
  HumanExplore::Status ();
  HumanChat::Status ();
  HumanArena::Status ();
  HumanAimCoach::Status ();
}
void HumanCopycat::LearningDamage (edict_t *victim, edict_t *attacker, int health, int armor) {
  if (!LearningEnabled () || learning_round_finished || !victim) return;
  if (attacker && !(attacker->v.flags & FL_CLIENT) && attacker->v.owner) attacker = attacker->v.owner;
  const int v = Slot (victim), a = Slot (attacker);
  const float damage = static_cast<float> (std::clamp (health, 0, 100)) + static_cast<float> (std::clamp (armor, 0, 100)) * 0.25f;
  if (damage <= 0) return;
  const auto reward = [&] (int slot, edict_t *entity, float amount) {
    if (slot < 0 || !entity || !bots[entity]) return;
    auto &l = Learner (entity, slot);
    if (l.pending && game.Time () - l.started < 1.5f) l.reward = Clamp (l.reward + amount, -3.0f, 3.0f);
  };
  const bool friendly = a >= 0 && v >= 0 && game.GetPlayerTeam (attacker) == game.GetPlayerTeam (victim);
  reward (v, victim, -std::min (damage * 0.012f, 1.0f));
  if (a >= 0 && a != v) reward (a, attacker, damage * (friendly ? -0.024f : 0.012f));
}
void HumanCopycat::LearningDeath (edict_t *killer, edict_t *victim) {
  if (!LearningEnabled () || learning_round_finished) return;
  const int v = Slot (victim), k = Slot (killer);
  if (v < 0 || death_seen[v]) return;
  death_seen[v] = true;
  if (bots[victim]) {
    auto &l = Learner (victim, v);
    if (l.pending) { l.reward -= 1.25f; CommitLearning (l, 0, true); }
  }
  if (k >= 0 && k != v && bots[killer]) {
    auto &l = Learner (killer, k);
    if (l.pending && game.Time () - l.started < 1.5f) {
      const bool friendly = game.GetPlayerTeam (killer) == game.GetPlayerTeam (victim);
      l.reward = Clamp (l.reward + (friendly ? -2.0f : 1.25f), -3.0f, 3.0f);
    }
  }
}
void HumanCopycat::LearningRoundEnd (Team winner) {
  if (!LearningEnabled () || learning_round_finished || (winner != Team::CT && winner != Team::Terrorist)) return;
  learning_round_finished = true;
  if (cv_learning_multiagent) for (unsigned t = 0; t < 2; ++t) if (team_experience[t])
    learning.finishTeam (t, t == static_cast<unsigned> (winner) ? 1.0f : -1.0f);
  for (auto &l : learners) if (l.pending) {
    if (cv_learning_multiagent) l.reward += l.team == static_cast<uint8_t> (winner) ? 0.35f : -0.35f;
    CommitLearning (l, 0, true);
  }
}
void HumanCopycat::LearningStatus () {
  const auto &s = learning.stats ();
  game.Print ("[Learning] enabled=%d rl=%d imitation=%d selfplay=%d lifelong=%d adaptation=%d multiagent=%d explore=%d%%",
    LearningEnabled () ? 1 : 0, cv_learning_rl.As<int> (), cv_learning_imitation.As<int> (), cv_learning_selfplay.As<int> (),
    cv_learning_lifelong.As<int> (), cv_learning_adaptation.As<int> (), cv_learning_multiagent.As<int> (), cv_learning_exploration.As<int> ());
  game.Print ("[Learning] Q_updates=%u human_samples=%u selfplay_updates=%u team_results=%u rounds=%u states=%u actions=%u",
    s.updates, s.humanSamples, s.selfPlayUpdates, s.teamRounds, s.rounds,
    static_cast<unsigned> (rbcc::LearningStates), static_cast<unsigned> (rbcc::LearningActions));
  for (unsigned t = 0; t < 2; ++t) {
    const auto &p = learning.opponent (t);
    game.Print ("[Learning] team=%s observed=%u pattern=%u changes=%u tactics(split/A/B)=%.3f/%.3f/%.3f",
      t == static_cast<unsigned> (Team::CT) ? "CT" : "T", p.observations, learning.pattern (t), p.changes,
      static_cast<double> (learning.tactic (t, rbcc::TeamTactic::Split).recent),
      static_cast<double> (learning.tactic (t, rbcc::TeamTactic::PressureA).recent),
      static_cast<double> (learning.tactic (t, rbcc::TeamTactic::PressureB).recent));
  }
}
} // namespace bot
#if defined(YSTL_TESTS)
namespace bot {
uint32_t HumanCopycat::TestAddClip (const rbcc::Clip &clip) { return database.add (clip); }
size_t HumanCopycat::TestClipCount () { return database.size (); }
bool HumanCopycat::TestReplaying (const Bot &bot) { return actors[bot.Index ()].replay.active (); }
size_t HumanCopycat::TestHumanNodes () { return human_routes.nodes ().size (); }
void HumanCopycat::TestRouteBudget (unsigned milliseconds) { route_budget_ms = std::clamp (milliseconds,1u,1000u); }
int HumanCopycat::TestRole (const Bot &bot) { return assignments[bot.Index ()].role; }
uint32_t HumanCopycat::TestEnvironment () { return environment; }
const char *HumanCopycat::TestDataPath () { return data_path.c_str (); }
uint32_t HumanCopycat::TestLearningUpdates () { return learning.stats ().updates; }
uint32_t HumanCopycat::TestLearningHumanSamples () { return learning.stats ().humanSamples; }
uint32_t HumanCopycat::TestLearningSelfPlay () { return learning.stats ().selfPlayUpdates; }
uint32_t HumanCopycat::TestLearningTeamRounds () { return learning.stats ().teamRounds; }
int HumanCopycat::TestLearningAction (const Bot &bot) { return learners[bot.Index ()].pending ? static_cast<int> (learners[bot.Index ()].action) : -1; }
uint32_t HumanCopycat::TestLearningObservations (Team team) { return learning.opponent (static_cast<unsigned> (team)).observations; }
}
#endif
