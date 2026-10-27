// Human Copycat FINAL 4.3.3. GPL-2.0-or-later.
#include "copycat/copycat_final.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <tuple>
#include <vector>
#include <aibo.h>
namespace bot {
namespace {
struct FinalActor {
  rbcc::FinalPersona persona {};
  uint32_t profile {};
  int serial { -1 }, user { -1 }, goal { -1 };
  float update {}, next_strategy {}, goal_until {}, stop_until {}, next_boost {}, next_brag {}, help_after {};
  float goal_best { 1e30f }, goal_progress {}, patrol_retry {};
  int patrol_zone { -1 }, last_patrol { -1 };
  unsigned patrol_visits[3] {};
  unsigned hunt_steps {};
  int hunt_continue { -1 };
  float next_hunt_look {}, hunt_yaw {};
  ystl::Vector stop {}, watch {};
  bool stopping {}, previous_top {}, boost_hold {};
};
struct Pair {
  int helper { -1 }, climber { -1 }, helper_serial { -1 }, climber_serial { -1 };
  ystl::Vector base {}, landing {}, approach {};
  rbcc::PhysicalBoost machine {};
  rbcc::BoostInput input {};
  float started {};
};
FinalActor actors[kGameMaxPlayers];
Pair pairs[2];
struct SmokeScreen {
  int thrower { -1 }, serial { -1 };
  ystl::Vector site {};
  float wait_until {}, retry_after {};
  bool defuse {};
};
SmokeScreen screens[2];
rbcc::FinalMemory memory;
std::vector<ystl::Vector> doors;
struct AnchorSight {
  float score {}, expires {};
  ystl::Vector look {};
};
std::map<std::tuple<int, int, int>, AnchorSight> sight_cache;
std::string map_name, memory_path;
uint32_t environment {}, run_seed { 1 };
bool ready {};
float next_join {}, next_leave {}, next_boost_scan {};
unsigned completed_boosts {}, failed_boosts {}, repeat_fallbacks {}, departures {}, arrivals {};
float death_time[kGameMaxPlayers] {};
int death_serial[kGameMaxPlayers] {};
rbcc::Vec Core (const ystl::Vector &v) {
  return { v.x, v.y, v.z };
}
ystl::Vector Feet (const Bot &b) {
  return b.pev->origin - ystl::Vector (0, 0, (b.pev->flags & FL_DUCKING) ? 18.0f : 36.0f);
}
uint32_t Profile (const Bot &b) {
  const auto name = b.pev->netname.str ();
  auto h = rbcc::hashBytes (name.chars (), name.size ());
  return h ? h : 1;
}
bool Alive (const Bot &b) {
  return b.is_alive_ && !b.Ent ()->free && b.pev->health > 0 && b.pev->deadflag == DEAD_NO;
}
bool Clear (const TraceResult &tr) {
  return !tr.fAllSolid && !tr.fStartSolid && tr.flFraction >= 0.999f;
}
FinalActor &Actor (Bot &b) {
  auto &a = actors[b.Index ()];
  const int user = engfuncs.pfnGetPlayerUserId (b.Ent ());
  const auto profile = Profile (b);
  if (a.serial != b.Ent ()->serialnumber || a.user != user || a.profile != profile) {
    HumanArena::CancelBoost (b.Index ());
    a = FinalActor {};
    a.serial = b.Ent ()->serialnumber;
    a.user = user;
    a.profile = profile;
    a.persona.seed (profile ^ run_seed ^ static_cast<uint32_t> (user));
    a.persona.choose ();
    a.next_strategy = game.Time () + 30.0f + static_cast<float> (a.persona.next () % 41u);
  }
  return a;
}
bool Urgent (const Bot &b) {
  return b.has_c4_ || b.is_vip_ || b.has_hostage_ || b.has_progress_bar_ || game_state.IsBombPlanted () || game_state.IsRoundOver () ||
         (game.MapIs (MapFlags::Demolition | MapFlags::HostageRescue | MapFlags::Assassination) && game_state.GetRoundTimeLeft () < 30);
}
float Rand (float low, float high) {
  return ystl::rg (low, high);
}
void CommandToward (Bot &b, HumanCommand &cmd, const ystl::Vector &foot, float speed) {
  auto heading = (foot - Feet (b)).angles ();
  heading.x = heading.z = 0;
  cmd.angles = heading;
  cmd.forward = speed;
  cmd.side = cmd.up = 0;
  cmd.buttons &= static_cast<uint16_t> (~(IN_ATTACK | IN_ATTACK2 | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_DUCK));
  if (speed > 1)
    cmd.buttons |= IN_FORWARD;
}
float SightScore (Bot &b, const ystl::Vector &at, ystl::Vector *look = nullptr) {
  const auto key = std::make_tuple (
    static_cast<int> (std::floor (at.x / 64.0f)), static_cast<int> (std::floor (at.y / 64.0f)), static_cast<int> (std::floor (at.z / 32.0f)));
  const auto cached = sight_cache.find (key);
  if (cached != sight_cache.end () && cached->second.expires > game.Time ()) {
    if (look)
      *look = cached->second.look;
    return cached->second.score;
  }
  const auto eye = at + ystl::Vector (0, 0, 54);
  float best = 0;
  ystl::Vector target {};
  for (const auto &door : doors) {
    const float d = door.distance (eye);
    if (d < 280 || d > 1800)
      continue;
    TraceResult tr {};
    engfuncs.pfnTraceLine (eye, door, 1, b.Ent (), &tr);
    if (!tr.fAllSolid && !tr.fStartSolid && (tr.flFraction > 0.94f || (tr.pHit && tr.pHit->v.classname.str ().starts_with ("func_door")))) {
      const float s = 420.0f - std::fabs (d - 850.0f) * 0.1f;
      if (s > best) {
        best = s;
        target = door;
      }
    }
  }
  // Public bombsite anchors plus actual geometry, never enemy coordinates.
  // From Dust2 Pit this can select the uphill A Long sightline toward A.
  if (game.MapIs (MapFlags::Demolition)) {
    unsigned checked = 0;
    for (const int node : graph.GetPoints (PointType::Goal)) {
      if (++checked > 16)
        break;
      if (!graph.Exists (node))
        continue;
      const auto site = graph[node].origin + ystl::Vector (0, 0, 25);
      const float distance = eye.distance (site);
      if (distance < 600 || distance > 5000)
        continue;
      TraceResult tr {};
      engfuncs.pfnTraceLine (eye, site, 1, b.Ent (), &tr);
      if (!Clear (tr))
        continue;
      const float score = 360.0f + std::min (distance * 0.04f, 90.0f) + (site.z > eye.z + 32 ? 40.0f : 0.0f);
      if (score > best) {
        best = score;
        target = site;
      }
    }
  }
  // Static double doors are often world geometry, not func_door entities.
  // A long center ray with obstructed parallel side rays identifies such gaps.
  for (unsigned i = 0; i < 8; ++i) {
    const ystl::Vector angles (0, static_cast<float> (i) * 45.0f, 0);
    const auto direction = angles.forward ();
    TraceResult tr {};
    engfuncs.pfnTraceLine (eye, eye + direction * 1400, 1, b.Ent (), &tr);
    float s = (!tr.fAllSolid && !tr.fStartSolid) ? tr.flFraction * 160.0f : 0;
    if (Clear (tr) && i % 2u == 0u) {
      TraceResult left {}, right {};
      const auto side = angles.right () * 36.0f;
      engfuncs.pfnTraceLine (eye - side, eye - side + direction * 1400, 1, b.Ent (), &left);
      engfuncs.pfnTraceLine (eye + side, eye + side + direction * 1400, 1, b.Ent (), &right);
      const float opening = std::clamp (1.0f - std::max (left.flFraction, right.flFraction), 0.0f, 1.0f);
      s += opening * 260.0f;
    }
    if (s > best) {
      best = s;
      target = eye + direction * 1000;
    }
    // A lower floor or a boosted perch needs a sloped ray, not just a flat ray.
    if (tr.flFraction < 0.8f) {
      for (const float pitch : { -6.0f, -12.0f, 6.0f, 12.0f }) {
        const auto slope = ystl::Vector (pitch, angles.y, 0).forward ();
        TraceResult elevated {};
        engfuncs.pfnTraceLine (eye, eye + slope * 1400, 1, b.Ent (), &elevated);
        const float score = (!elevated.fAllSolid && !elevated.fStartSolid) ? elevated.flFraction * 240.0f : 0;
        if (score > best) {
          best = score;
          target = eye + slope * 1000;
        }
      }
    }
  }
  ystl::Vector watch {};
  if (best > 0) {
    watch = (target - eye).angles ();
    watch.x = -watch.x;
  }
  if (look)
    *look = watch;
  if (sight_cache.size () < 2048)
    sight_cache[key] = { best, game.Time () + 30.0f, watch };
  return best;
}
}
bool HumanArena::Enabled () {
  return ready && cv_final_brain && HumanCopycat::Enabled ();
}
void HumanArena::Start (const char *map, uint32_t physics, const char *data) {
  Stop ();
  map_name = map;
  environment = physics;
  const std::string base = data;
  memory_path = base.substr (0, base.size () - 5) + ".hfinal";
  ready = true;
  run_seed = static_cast<uint32_t> (ystl::rg (1, 0x7fffffff));
  next_join = next_leave = next_boost_scan = 0;
  completed_boosts = failed_boosts = repeat_fallbacks = departures = arrivals = 0;
  std::string error;
  std::error_code ec;
  std::fill (std::begin (death_time), std::end (death_time), -100.0f);
  std::fill (std::begin (death_serial), std::end (death_serial), -1);
  if (cv_final_memory && !memory.load (memory_path, map_name, environment, error) && std::filesystem::exists (memory_path, ec))
    game.Print ("[FINAL] Ignored memory: %s", error.c_str ());
  doors.clear ();
  sight_cache.clear ();
  for (const char *kind : { "func_door", "func_door_rotating" })
    game.SearchEntities ("classname", kind, [] (edict_t *e) {
      if (doors.size () >= 64)
        return EntitySearchResult::Break;
      const auto center = (e->v.absmin + e->v.absmax) * 0.5f;
      doors.push_back (center.empty () ? e->v.origin : center);
      return EntitySearchResult::Continue;
    });
}
void HumanArena::Stop () {
  if (ready) {
    Save ();
    for (int i = 0; i < kGameMaxPlayers; ++i)
      CancelBoost (i);
  }
  for (int i = 0; i < kGameMaxPlayers; ++i)
    actors[i] = FinalActor {};
  for (auto &p : pairs)
    p = Pair {};
  for (auto &screen : screens)
    screen = SmokeScreen {};
  memory.clear ();
  doors.clear ();
  sight_cache.clear ();
  ready = false;
}
bool HumanArena::Save () {
  if (!ready || !cv_final_memory || !memory.dirty ())
    return true;
  std::error_code ec;
  std::filesystem::create_directories (std::filesystem::path (memory_path).parent_path (), ec);
  std::string error;
  if (ec || !memory.save (memory_path, map_name, environment, error)) {
    game.Print ("[FINAL] Memory save failed: %s", ec ? ec.message ().c_str () : error.c_str ());
    return false;
  }
  return true;
}
bool HumanArena::Dirty () {
  return ready && cv_final_memory && memory.dirty ();
}
void HumanArena::NewRound () {
  for (auto &screen : screens)
    screen = SmokeScreen {};
  for (auto &b : bots) {
    CancelBoost (b.Index ());
    if (!Enabled ())
      continue;
    auto &a = Actor (b);
    a.persona.choose ();
    memory.strategy (a.profile, a.persona.current);
    a.goal = -1;
    a.patrol_zone = -1;
    a.hunt_steps = 0;
    a.hunt_continue = -1;
    a.stopping = false;
    a.boost_hold = false;
    a.next_strategy = game.Time () + 35.0f + static_cast<float> (a.persona.next () % 46u);
  }
}
void HumanArena::ResetBot (Bot &b) {
  if (!ready)
    return;
  CancelBoost (b.Index ());
  auto &a = Actor (b);
  a.goal = -1;
  a.patrol_zone = -1;
  a.hunt_steps = 0;
  a.hunt_continue = -1;
  a.stopping = false;
  a.boost_hold = false;
  a.stop_until = 0;
}
void HumanArena::Tune (Bot &b) {
  if (!Enabled () || !cv_final_emotions)
    return;
  const auto &p = Actor (b).persona;
  b.agression_level_ = std::clamp (0.15f + p.risk * 0.85f, 0.0f, 1.0f);
  b.fear_level_ = std::clamp (p.fear * 0.7f + (1 - p.risk) * 0.4f, 0.0f, 1.0f);
}
void HumanArena::BeforeThink (Bot &b) {
  if (!Enabled ())
    return;
  auto &a = Actor (b);
  const float now = game.Time ();
  if (a.patrol_zone >= 0 && !PatrolDuty (b)) {
    a.goal = -1;
    a.patrol_zone = -1;
  }
  if (now >= a.update) {
    const float dt = a.update > 0 ? std::clamp (now - a.update + 0.75f, 0.0f, 2.0f) : 0.75f;
    a.update = now + 0.75f;
    const int score = static_cast<int> (b.pev->frags);
    int leader = score, rank = 1, participants = 0;
    for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i) {
      auto e = game.PlayerOfIndex (i);
      if (!e || e->free || !(e->v.flags & FL_CLIENT))
        continue;
      const auto team = game.GetPlayerTeam (e);
      if (team != Team::CT && team != Team::Terrorist)
        continue;
      ++participants;
      const int other = static_cast<int> (e->v.frags);
      leader = std::max (leader, other);
      if (other > score)
        ++rank;
    }
    a.persona.update (dt, static_cast<int> (b.pev->health), score, leader, rank);
    if (participants < 2)
      a.persona.top = false;
    if (a.persona.top && !a.previous_top && now >= a.next_brag) {
      a.next_brag = now + 45;
      if (Alive (b))
        HumanChat::Queue (b, Chat::NoKeyword, false);
    }
    a.previous_top = a.persona.top;
    if (now >= a.next_strategy && !Urgent (b) && a.patrol_zone < 0) {
      a.persona.choose ();
      memory.strategy (a.profile, a.persona.current);
      a.next_strategy = now + 35 + static_cast<float> (a.persona.next () % 46u);
      a.goal = -1;
    }
  }
  Tune (b);
  if (Boosting (b))
    b.IgnoreCollision ();
  if (b.GetTaskId () != TaskId::Camp && b.GetTaskId () != TaskId::Hide) {
    a.stopping = false;
    a.boost_hold = false;
  }
  if (graph.Exists (a.goal)) {
    const float d = b.pev->origin.distance (graph[a.goal].origin);
    if (d + 24.0f < a.goal_best) {
      a.goal_best = d;
      a.goal_progress = now;
    }
    else if (now - a.goal_progress > 6.0f && !Urgent (b) && b.GetTaskId () == TaskId::Normal) {
      Visit (b, graph[a.goal].origin - ystl::Vector (0, 0, 36), 0);
      a.goal = -1;
      a.patrol_zone = -1;
      a.patrol_retry = now + 3.0f;
      b.ClearSearchNodes ();
      b.Task ()->data = kInvalidNodeIndex;
    }
  }
  if (a.goal >= 0 && graph.Exists (a.goal) && Feet (b).distance (graph[a.goal].origin - ystl::Vector (0, 0, 36)) < 85) {
    const int visited_patrol = a.patrol_zone;
    if (visited_patrol >= 0 && visited_patrol == PatrolRegion (Feet (b))) {
      Visit (b, Feet (b), 0);
      if (++a.hunt_steps < 3)
        a.hunt_continue = visited_patrol;
      else {
        ++a.patrol_visits[visited_patrol];
        a.last_patrol = visited_patrol;
        a.hunt_steps = 0;
        a.hunt_continue = -1;
      }
    }
    a.patrol_zone = -1;
    if (b.current_weapon_ == Weapon::AWP && !Urgent (b) && b.GetTaskId () == TaskId::Normal && game.IsNullEntity (b.enemy_) &&
        (a.persona.current == rbcc::FinalStrategy::Snipe || visited_patrol == 2)) {
      b.StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, now + Rand (2.5f, 7.0f), true);
    }
    else
      Visit (b, Feet (b), 0);
    a.goal = -1;
  }
  if (cv_final_boost && now >= next_boost_scan && !Boosting (b)) {
    TryBoost (b);
  }
}
bool HumanArena::Fresh (const Bot &b, const ystl::Vector &foot) {
  return !Enabled () || !cv_final_variety || memory.fresh (Profile (b), Core (foot));
}
float HumanArena::PlaceBias (const Bot &b, const ystl::Vector &foot) {
  float value = Enabled () && cv_final_variety ? memory.bias (Profile (b), Core (foot)) : 0.0f;
  if (PatrolDuty (b)) {
    const auto &a = actors[b.Index ()];
    const int zone = PatrolRegion (foot);
    if (zone >= 0)
      value += 650.0f / (1.0f + static_cast<float> (a.patrol_visits[zone])) + (zone != a.last_patrol ? 100.0f : 0.0f);
    else {
      // Bias a real discovered frontier toward a rare area; never invent an
      // overlay node or bypass its floor/hull/connected-path validation.
      const ystl::Vector anchors[] {
        { -250,  -750, 70   },
        { -1650, 1050, 20   },
        { 1450,  180,  -192 }
      };
      float frontier_bias = 0;
      for (int i = 0; i < 3; ++i)
        frontier_bias =
          std::max (frontier_bias, 220.0f - foot.distance2d (anchors[i]) * 0.05f - static_cast<float> (a.patrol_visits[i]) * 20.0f);
      value += frontier_bias;
    }
  }
  return value;
}
void HumanArena::Visit (Bot &b, const ystl::Vector &foot, int use) {
  if (Enabled () && cv_final_variety && use >= 0 && use <= 4)
    memory.remember (Actor (b).profile, Core (foot), static_cast<rbcc::PlaceUse> (use));
}
bool HumanArena::PatrolMode (const Bot &b) {
  return Enabled () && cv_human_ct_patrol && map_name == "de_dust2" && b.team_ == Team::CT;
}
int HumanArena::PatrolRegion (const ystl::Vector &p) {
  // Standard CS 1.6 Dust2 coordinates verified against the upstream 1238-node
  // graph. Inputs are feet; no map node number, enemy coordinate or teleport.
  if (p.x >= -850 && p.x <= 450 && p.y >= -1050 && p.y <= -420)
    return 0;
  if (p.x >= -2100 && p.x <= -980 && p.y >= -220 && p.y <= 1510)
    return 1;
  if (p.x >= 1220 && p.x <= 1660 && p.y >= -80 && p.y <= 420 && p.z >= -240 && p.z <= -70)
    return 2;
  return -1;
}
bool HumanArena::PatrolDuty (const Bot &b) {
  if (!PatrolMode (b) || !Alive (b) || Urgent (b) || b.pev->health < 35 || !b.buying_finished_ || !game.IsNullEntity (b.enemy_) ||
      has_flag (b.states_, Sense::SeeingEnemy | Sense::HearingEnemy) || Boosting (b))
    return false;
  unsigned count = 0, order = 0;
  for (const auto &mate : bots)
    if (mate.team_ == Team::CT && Alive (mate) && !mate.is_vip_ && !mate.has_hostage_) {
      if (mate.Index () < b.Index ())
        ++order;
      ++count;
    }
  if (!count)
    return false;
  const unsigned patrols = count > 1 ? std::min ((count + 1) / 2, count - 1) : 1;
  return (order + count - HumanCopycat::RoundNumber () % count) % count < patrols;
}
bool HumanArena::PatrolActive (const Bot &b) {
  const auto &a = actors[b.Index ()];
  return PatrolDuty (b) && a.patrol_zone >= 0 && graph.Exists (a.goal) && game.Time () < a.goal_until;
}
int HumanArena::PatrolGoal (Bot &b) {
  if (!PatrolDuty (b) || cv_debug_goal.As<int> () >= 0 || b.GetTaskId () != TaskId::Normal)
    return -1;
  auto &a = Actor (b);
  if (a.patrol_zone >= 0 && graph.Exists (a.goal) && game.Time () < a.goal_until)
    return a.goal;
  if (game.Time () < a.patrol_retry)
    return -1;
  a.patrol_zone = -1;
  a.patrol_retry = game.Time () + 2.0f;
  const int count = graph.Length ();
  if (count < 2)
    return -1;
  const int start = graph.Exists (b.current_node_index_) ? b.current_node_index_ : graph.GetNearest (b.pev->origin);
  if (!graph.Exists (start))
    return -1;
  std::vector<uint8_t> reachable (static_cast<size_t> (count));
  std::vector<int> queue { start };
  reachable[static_cast<size_t> (start)] = 1;
  for (size_t cursor = 0; cursor < queue.size (); ++cursor)
    for (const auto &link : graph[queue[cursor]].links) {
      const int to = link.index;
      if (!graph.Exists (to) || reachable[static_cast<size_t> (to)] || mode_walls.IsNodeBlocked (graph[to].origin))
        continue;
      reachable[static_cast<size_t> (to)] = 1;
      queue.push_back (to);
    }
  int candidates[3] { -1, -1, -1 };
  float scores[3] { -1e30f, -1e30f, -1e30f };
  // Scan every reachable node, not the old random sample of 96. This includes
  // late-numbered spawn/tunnel/pit nodes, without depending on a specific GRAPH.
  for (const int node : queue) {
    const auto &path = graph[node];
    const auto foot = path.origin - ystl::Vector (0, 0, 36);
    const int zone = PatrolRegion (foot);
    if (zone < 0 || node == start || path.origin.distance2d (b.pev->origin) < 160 ||
        has_flag (path.flags, NodeFlag::Ladder | NodeFlag::TerroristOnly) || mode_walls.IsNodeBlocked (path.origin) || !Fresh (b, foot))
      continue;
    const float score = memory.bias (a.profile, Core (foot)) + static_cast<float> (a.persona.next () % 101u) - foot.distance (Feet (b)) * 0.015f;
    if (score > scores[zone]) {
      scores[zone] = score;
      candidates[zone] = node;
    }
  }
  int chosen = -1;
  float best = 1e30f;
  const unsigned rotate = (static_cast<unsigned> (b.Index ()) + HumanCopycat::RoundNumber ()) % 3u;
  for (unsigned turn = 0; turn < 3; ++turn) {
    const int zone = static_cast<int> ((turn + rotate) % 3u);
    if (candidates[zone] < 0)
      continue;
    unsigned busy = 0;
    for (const auto &mate : bots)
      if (&mate != &b && mate.team_ == Team::CT && Alive (mate) && actors[mate.Index ()].patrol_zone == zone &&
          actors[mate.Index ()].goal_until > game.Time ())
        ++busy;
    const float cost = static_cast<float> (a.patrol_visits[zone]) * 100.0f + static_cast<float> (busy) * 45.0f +
                       (zone == a.last_patrol ? 60.0f : 0.0f) - (zone == a.hunt_continue ? 1000.0f : 0.0f);
    if (cost < best) {
      best = cost;
      chosen = zone;
    }
  }
  if (chosen < 0)
    return -1; // geometry exploration can handle an unmapped area
  if (chosen != a.hunt_continue)
    a.hunt_steps = 0;
  a.hunt_continue = chosen;
  a.goal = candidates[chosen];
  a.patrol_zone = chosen;
  a.goal_until = game.Time () + 50.0f;
  a.goal_best = 1e30f;
  a.goal_progress = game.Time ();
  return a.goal;
}
int HumanArena::Goal (Bot &b) {
  if (const int patrol = PatrolGoal (b); graph.Exists (patrol))
    return patrol;
  if (!Enabled () || !cv_final_variety || !Alive (b) || Urgent (b) || !b.buying_finished_ || cv_debug_goal.As<int> () >= 0 || Boosting (b) ||
      HumanExplore::Scout (b))
    return kInvalidNodeIndex;
  auto &a = Actor (b);
  if (a.persona.current == rbcc::FinalStrategy::Support)
    return kInvalidNodeIndex;
  if (graph.Exists (a.goal) && game.Time () < a.goal_until)
    return a.goal;
  const int count = graph.Length ();
  if (count < 2)
    return kInvalidNodeIndex;
  ystl::Vector contact {};
  edict_t *enemy = nullptr;
  int seen = 0;
  const bool has_contact = bots.GetNearestTeamContact (b.team_, b.pev->origin, enemy, contact, 6.0f, &seen);
  int best = -1;
  float highest = -1e30f;
  for (int pass = 0; pass < 2 && best < 0; ++pass)
    for (int n = 0; n < std::min (count, 96); ++n) {
      const int node = count <= 96 ? n : static_cast<int> (a.persona.next () % static_cast<uint32_t> (count));
      if (!graph.Exists (node) || node == b.current_node_index_ || has_flag (graph[node].flags, NodeFlag::Ladder) ||
          mode_walls.IsNodeBlocked (graph[node].origin))
        continue;
      const auto foot = graph[node].origin - ystl::Vector (0, 0, 36);
      const float distance = foot.distance (Feet (b));
      if (distance < 160)
        continue;
      if (pass == 0 && !Fresh (b, foot))
        continue;
      float score = PlaceBias (b, foot) + static_cast<float> (a.persona.next () % 301u) - distance * 0.025f;
      if (has_contact && (a.persona.current == rbcc::FinalStrategy::Push || a.persona.current == rbcc::FinalStrategy::Flank))
        score += 400.0f - contact.distance (graph[node].origin) * 0.2f;
      if (a.persona.current == rbcc::FinalStrategy::Scout)
        score += distance * 0.07f;
      if (a.persona.current == rbcc::FinalStrategy::Lurk)
        score += has_flag (graph[node].flags, NodeFlag::Camp) ? 100.0f : 0.0f;
      if (b.current_weapon_ == Weapon::AWP && (a.persona.current == rbcc::FinalStrategy::Snipe || a.persona.risk < 0.55f)) {
        if (has_flag (graph[node].flags, NodeFlag::Sniper | NodeFlag::Camp))
          score += 150;
        if (n < 20)
          score += SightScore (b, foot) * 0.8f;
      }
      if (score > highest) {
        highest = score;
        best = node;
      }
    }
  if (best >= 0) {
    const auto foot = graph[best].origin - ystl::Vector (0, 0, 36);
    if (!Fresh (b, foot))
      ++repeat_fallbacks;
    a.goal = best;
    a.goal_until = game.Time () + Rand (20, 40);
    a.goal_best = 1e30f;
    a.goal_progress = game.Time ();
  }
  return best;
}
bool HumanArena::Camp (Bot &b, bool hide) {
  if (!Enabled () || !cv_final_variety)
    return true;
  auto &a = Actor (b);
  const auto foot = Feet (b);
  const bool emergency = Urgent (b) || (hide && (b.pev->health < 40 || has_flag (b.states_, Sense::SeeingEnemy)));
  if (!a.stopping || a.stop.distance_sq (foot) > 96.0f * 96.0f) {
    if (!emergency && !Fresh (b, foot))
      return false;
    a.stopping = true;
    a.stop = foot;
    a.stop_until = game.Time () + Rand (hide ? 2.0f : 2.5f, hide ? 6.0f : 8.0f) * (1.15f - a.persona.risk * 0.4f);
    Visit (b, foot, hide ? 2 : (b.current_weapon_ == Weapon::AWP ? 3 : 1));
    SightScore (b, foot, &a.watch);
  }
  if (!emergency && game.Time () > a.stop_until) {
    a.stopping = false;
    a.goal = -1;
    b.ClearSearchNodes ();
    return false;
  }
  return true;
}
void HumanArena::IdleLook (Bot &b, ystl::Vector &desired) {
  if (PatrolActive (b) && b.GetTaskId () == TaskId::Normal && game.IsNullEntity (b.enemy_)) {
    auto &a = Actor (b);
    if (game.Time () >= a.next_hunt_look) {
      a.next_hunt_look = game.Time () + 0.8f;
      a.hunt_yaw = (a.persona.next () % 3u == 0u ? -32.0f : a.persona.next () % 3u == 0u ? 32.0f : 0.0f);
    }
    // Small, ordinary view sweeps while advancing; combat/hearing immediately
    // takes priority. The aim controller still limits all turn speed/error.
    auto sight = desired;
    sight.y += a.hunt_yaw;
    TraceResult tr {};
    engfuncs.pfnTraceLine (b.GetEyesPos (), b.GetEyesPos () + ystl::Vector (-sight.x, sight.y, 0).forward () * 500, 1, b.Ent (), &tr);
    if (!tr.fStartSolid && !tr.fAllSolid && tr.flFraction > 0.25f)
      desired = sight;
  }
  if (Enabled () && (b.current_weapon_ == Weapon::AWP || actors[b.Index ()].boost_hold) && game.IsNullEntity (b.enemy_) &&
      b.GetTaskId () == TaskId::Camp) {
    auto &a = Actor (b);
    if (a.stopping)
      desired = a.watch;
  }
}
float HumanArena::SniperWatch (Bot &b, const ystl::Vector &foot, ystl::Vector &desired) {
  return Enabled () ? SightScore (b, foot, &desired) : 0;
}
bool HumanArena::ObjectiveSmoke (Bot &b, bool defuse) {
  if (!ready || !HumanCopycat::Enabled () || !cv_human_objective_smoke || !game.MapIs (MapFlags::Demolition) || game_state.IsRoundOver () ||
      !Alive (b) || b.has_progress_bar_ ||
      (defuse ? b.team_ != Team::CT || !game_state.IsBombPlanted () : b.team_ != Team::Terrorist || !b.has_c4_ || !b.in_bomb_zone_))
    return false;
  const auto site = defuse ? game_state.GetBombOrigin () : b.pev->origin;
  if (defuse && (site.empty () || b.pev->origin.distance_sq (site) > 128.0f * 128.0f))
    return false;
  if (HumanCopycat::SmokeCover (site + ystl::Vector (0, 0, 24)))
    return false;
  const float now = game.Time ();
  const float remaining = defuse ? game_state.GetBombTimeLeft () : static_cast<float> (game_state.GetRoundTimeLeft ());
  const float need = defuse ? (b.has_defuser_ ? 7.0f : 12.0f) : 4.0f;
  const bool can_wait = remaining > need + 6.0f;
  auto &screen = screens[b.team_ == Team::CT ? 1 : 0];
  if (screen.retry_after > now && screen.defuse == defuse && screen.site.distance_sq (site) < 180.0f * 180.0f) {
    const auto thrower = bots.FindBotByIndex (screen.thrower);
    return can_wait && now < screen.wait_until && thrower && Alive (*thrower) && thrower->Ent ()->serialnumber == screen.serial;
  }
  if (screen.wait_until > now)
    return false; // another worksite already has this team's one active request

  // Offset the screen toward a recent public callout, otherwise a known approach.
  // Keep it close enough to cover the worker even if no contact has been heard.
  ystl::Vector direction {};
  edict_t *contact = nullptr;
  ystl::Vector heard {};
  if (bots.GetNearestTeamContact (b.team_, site, contact, heard, 4.0f))
    direction = heard - site;
  else if (!b.look_at_.empty ())
    direction = b.look_at_ - site;
  direction.z = 0;
  if (direction.length2d () < 1)
    direction = ystl::Vector (0, b.pev->v_angle.y, 0).forward ();
  auto target = site + direction.normalize () * 72.0f;
  TraceResult floor {};
  engfuncs.pfnTraceLine (target + ystl::Vector (0, 0, 40), target - ystl::Vector (0, 0, 100), 1, b.Ent (), &floor);
  if (floor.fStartSolid || floor.fAllSolid || floor.flFraction >= 0.999f || floor.vecPlaneNormal.z < 0.65f ||
      std::fabs (floor.vecEndPos.z - site.z) > 64)
    return false;
  target = floor.vecEndPos + ystl::Vector (0, 0, 8);
  const int contents = engfuncs.pfnPointContents (target);
  if (contents == CONTENTS_SOLID || contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
    return false;

  Bot *selected = nullptr;
  float nearest = 1e30f;
  for (auto &candidate : bots) {
    if (candidate.team_ != b.team_ || !Alive (candidate) || candidate.has_progress_bar_ || Boosting (candidate) || candidate.is_using_grenade_ ||
        candidate.reload_data_.is_reloading || !candidate.buying_finished_ || candidate.GetTaskId () == TaskId::ThrowFlashbang ||
        candidate.GetTaskId () == TaskId::ThrowExplosive || candidate.GetTaskId () == TaskId::ThrowSmoke ||
        !has_flag (candidate.pev->weapons, ystl::bit (Weapon::Smoke)) || candidate.IsWeaponRestricted (Weapon::Smoke) ||
        candidate.IsOnLadder () || candidate.IsInWater () ||
        (&candidate != &b && (candidate.HasBombTask () || candidate.has_c4_ || candidate.is_vip_ || candidate.has_hostage_)) ||
        (&candidate == &b && !can_wait))
      continue;
    const float distance = candidate.pev->origin.distance (site);
    if (distance > 700 || (!game.IsNullEntity (candidate.enemy_) && distance < 250) || !candidate.IsGrenadeTargetSafe (Weapon::Smoke, target))
      continue;
    auto velocity = candidate.CalcThrow (candidate.GetEyesPos (), target);
    if (velocity.length_sq () < 100)
      velocity = candidate.CalcToss (candidate.GetEyesPos (), target);
    if (velocity.length_sq () <= 100)
      continue;
    const float score = distance + (&candidate == &b ? 1000.0f : 0.0f);
    if (score < nearest) {
      selected = &candidate;
      nearest = score;
    }
  }
  if (!selected)
    return false;
  screen = { selected->Index (), selected->Ent ()->serialnumber, site, now + (can_wait ? 4.5f : 0.0f), now + 12.0f, defuse };
  selected->throw_ = target;
  selected->states_ |= Sense::ThrowSmoke;
  // Preserve the resumable PlantBomb/DefuseBomb task under the normal throw task.
  selected->StartTask (TaskId::ThrowSmoke, TaskPri::kThrow, kInvalidNodeIndex, now + 4.0f, false);
  return can_wait;
}
int HumanArena::Mood (const Bot &b) {
  return Enabled () ? static_cast<int> (actors[b.Index ()].persona.mood) : 0;
}
bool HumanArena::Top (const Bot &b) {
  if (!Enabled () || b.pev->frags <= 0)
    return false;
  int participants = 0;
  for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i) {
    auto e = game.PlayerOfIndex (i);
    if (!e || e->free || !(e->v.flags & FL_CLIENT))
      continue;
    const auto team = game.GetPlayerTeam (e);
    if (team != Team::CT && team != Team::Terrorist)
      continue;
    ++participants;
    if (e->v.frags > b.pev->frags)
      return false;
  }
  return participants >= 2;
}
int HumanArena::Rank (const Bot &b) {
  return Enabled () ? actors[b.Index ()].persona.rank : 1;
}
float HumanArena::Risk (const Bot &b) {
  return Enabled () ? actors[b.Index ()].persona.risk : 0.5f;
}
void HumanArena::Damage (edict_t *victim, edict_t *, int damage) {
  if (!Enabled () || !cv_final_emotions)
    return;
  if (auto b = bots[victim])
    Actor (*b).persona.hurt (static_cast<float> (std::clamp (damage, 0, 100)));
}
void HumanArena::Death (edict_t *killer, edict_t *victim) {
  if (!Enabled () || !victim || victim->free || !(victim->v.flags & FL_CLIENT))
    return;
  const int n = game.IndexOfPlayer (victim);
  if (n < 0 || n >= kGameMaxPlayers)
    return;
  if (death_serial[n] == victim->serialnumber && game.Time () - death_time[n] < 0.1f)
    return;
  death_serial[n] = victim->serialnumber;
  death_time[n] = game.Time ();
  if (auto b = bots[victim]) {
    auto &a = Actor (*b);
    a.persona.died ();
    a.persona.choose ();
    memory.strategy (a.profile, a.persona.current);
    a.next_strategy = game.Time () + Rand (25, 60);
  }
  if (killer != victim)
    if (auto b = bots[killer])
      Actor (*b).persona.killed (game.GetPlayerTeam (killer) == game.GetPlayerTeam (victim));
}
bool HumanArena::Boosting (const Bot &b) {
  for (const auto &p : pairs)
    if (p.helper == b.Index () || p.climber == b.Index ())
      return true;
  return false;
}
bool HumanArena::VantageHolding (Bot &b) {
  const auto &a = actors[b.Index ()];
  return Enabled () && a.boost_hold && a.stopping && b.GetTaskId () == TaskId::Camp && game.Time () < a.stop_until;
}
void HumanArena::CancelBoost (int slot) {
  for (auto &p : pairs)
    if (p.helper == slot || p.climber == slot) {
      for (int s : { p.helper, p.climber })
        if (s >= 0 && s < kGameMaxPlayers) {
          actors[s].next_boost = game.Time () + Rand (25, 60);
          actors[s].help_after = actors[s].next_boost;
          if (auto b = bots.FindBotByIndex (s))
            b->ClearTask (TaskId::Pause);
        }
      p = Pair {};
    }
}
bool HumanArena::TryBoost (Bot &b) {
  if (!Enabled () || !cv_final_boost || !Alive (b) || Urgent (b) || b.GetTaskId () != TaskId::Normal || !b.IsOnFloor () || b.IsInWater () ||
      b.IsOnLadder () || !game.IsNullEntity (b.enemy_) || has_flag (b.states_, Sense::SeeingEnemy | Sense::HearingEnemy) ||
      game.Time () < Actor (b).next_boost)
    return false;
  const auto team = static_cast<unsigned> (b.team_);
  if (team > 1 || pairs[team].helper >= 0)
    return false;
  next_boost_scan = game.Time () + 0.25f;
  Actor (b).next_boost = game.Time () + Rand (6, 12);
  const auto foot = Feet (b);
  Bot *helper = nullptr;
  float nearest = 300;
  for (auto &mate : bots) {
    if (&mate == &b || mate.team_ != b.team_ || !Alive (mate) || Urgent (mate) || Boosting (mate) || mate.GetTaskId () != TaskId::Normal ||
        !mate.IsOnFloor () || mate.IsInWater () || mate.IsOnLadder () || !game.IsNullEntity (mate.enemy_) ||
        game.Time () < Actor (mate).help_after)
      continue;
    const float d = mate.pev->origin.distance (b.pev->origin);
    if (d < nearest) {
      nearest = d;
      helper = &mate;
    }
  }
  if (!helper)
    return false;
  for (unsigned k = 0; k < 8; ++k) {
    const float yaw = static_cast<float> ((Actor (b).persona.next () % 8u) * 45u);
    const auto direction = ystl::Vector (0, yaw, 0).forward ();
    const auto horizontal = foot + direction * 96.0f;
    TraceResult floor {};
    engfuncs.pfnTraceLine (horizontal + ystl::Vector (0, 0, 156), horizontal + ystl::Vector (0, 0, 52), 1, b.Ent (), &floor);
    if (floor.fAllSolid || floor.fStartSolid || floor.flFraction >= 0.999f || floor.vecPlaneNormal.z < 0.7f)
      continue;
    const auto landing = floor.vecEndPos;
    const float rise = landing.z - foot.z;
    if (rise < 60 || rise > 132 || !Fresh (b, landing))
      continue;
    const auto base = horizontal - direction * 48.0f;
    TraceResult ground {};
    engfuncs.pfnTraceLine (base + ystl::Vector (0, 0, 18), base - ystl::Vector (0, 0, 32), 1, b.Ent (), &ground);
    if (ground.fAllSolid || ground.fStartSolid || ground.flFraction >= 0.999f || ground.vecPlaneNormal.z < 0.7f)
      continue;
    const auto base_foot = ground.vecEndPos;
    if (!HumanExplore::Walkable (*helper, Feet (*helper), base_foot) || !HumanExplore::Walkable (b, foot, base_foot - direction * 40.0f))
      continue;
    TraceResult hull {};
    const auto top = landing + ystl::Vector (0, 0, 36);
    engfuncs.pfnTraceHull (top, top, 1, human_hull, b.Ent (), &hull);
    if (!Clear (hull))
      continue;
    const auto arc = base_foot + ystl::Vector (0, 0, std::max (150.0f, rise + 28.0f));
    engfuncs.pfnTraceHull (base_foot + ystl::Vector (0, 0, 36), arc, 1, human_hull, b.Ent (), &hull);
    if (!Clear (hull))
      continue;
    engfuncs.pfnTraceHull (arc, top, 1, head_hull, b.Ent (), &hull);
    if (!Clear (hull))
      continue;
    auto &p = pairs[team];
    p.helper = helper->Index ();
    p.climber = b.Index ();
    p.helper_serial = helper->Ent ()->serialnumber;
    p.climber_serial = b.Ent ()->serialnumber;
    p.base = base_foot;
    p.landing = landing;
    p.approach = base_foot - direction * 40.0f;
    p.started = game.Time ();
    p.machine.start (p.started, rise);
    helper->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, p.started + 10, true);
    b.StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, p.started + 10, true);
    return true;
  }
  return false;
}
bool HumanArena::Motor (Bot &b, HumanCommand &cmd, ystl::Vector &desired, bool available) {
  if (!Boosting (b))
    return false;
  Pair *pair = nullptr;
  for (auto &p : pairs)
    if (p.helper == b.Index () || p.climber == b.Index ()) {
      pair = &p;
      break;
    }
  if (!pair)
    return false;
  auto &p = *pair;
  auto helper = bots.FindBotByIndex (p.helper), climber = bots.FindBotByIndex (p.climber);
  if (!Enabled () || !cv_final_boost || !available || !helper || !climber || !Alive (*helper) || !Alive (*climber) ||
      helper->Ent ()->serialnumber != p.helper_serial || climber->Ent ()->serialnumber != p.climber_serial || helper->team_ != climber->team_ ||
      Urgent (*helper) || Urgent (*climber) || !game.IsNullEntity (helper->enemy_) || !game.IsNullEntity (climber->enemy_) ||
      has_flag (helper->states_, Sense::SeeingEnemy | Sense::HearingEnemy) ||
      has_flag (climber->states_, Sense::SeeingEnemy | Sense::HearingEnemy)) {
    ++failed_boosts;
    CancelBoost (b.Index ());
    return false;
  }
  const auto hf = Feet (*helper), cf = Feet (*climber);
  const bool supported = climber->pev->groundentity == helper->Ent () && (climber->pev->flags & FL_ONGROUND) && cf.z - hf.z > 28;
  rbcc::BoostObservation o;
  o.helperReady = hf.distance (p.base) < 14;
  o.climberReady = cf.distance (p.approach) < 18;
  o.supported = supported;
  o.helperStanding = (helper->pev->flags & FL_DUCKING) == 0;
  o.height = cf.z - hf.z;
  o.landed = (climber->pev->flags & FL_ONGROUND) && climber->pev->groundentity != helper->Ent () && std::fabs (cf.z - p.landing.z) < 10 &&
             cf.distance (p.landing) < 52;
  o.danger = p.machine.phase () != rbcc::BoostPhase::Approach && hf.distance (p.base) > 48;
  p.input = p.machine.update (game.Time (), o);
  if (p.input.phase == rbcc::BoostPhase::Done) {
    const auto landing = p.landing;
    Visit (*helper, p.base, 4);
    Visit (*climber, p.landing, 4);
    ++completed_boosts;
    CancelBoost (b.Index ());
    // Use the earned vantage briefly before resuming varied navigation.
    auto &a = Actor (*climber);
    a.stopping = a.boost_hold = true;
    a.stop = landing;
    a.stop_until = game.Time () + Rand (3.0f, 6.5f);
    SightScore (*climber, landing, &a.watch);
    climber->ClearSearchNodes ();
    climber->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, a.stop_until, true);
    cmd.forward = cmd.side = cmd.up = 0;
    cmd.buttons &= static_cast<uint16_t> (~(IN_JUMP | IN_DUCK | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
    return true;
  }
  if (p.input.phase == rbcc::BoostPhase::Aborted) {
    ++failed_boosts;
    CancelBoost (b.Index ());
    return false;
  }
  const bool base = b.Index () == p.helper;
  ystl::Vector target =
    base ? p.base : (p.input.phase == rbcc::BoostPhase::Approach ? p.approach : (p.input.towardLanding ? p.landing : p.base));
  const auto delta = target - Feet (b);
  const float speed = delta.length2d () > (base ? 8.0f : 12.0f) ? (base ? 110.0f : 170.0f) : 0.0f;
  if (p.input.phase == rbcc::BoostPhase::Approach && !HumanExplore::Walkable (b, Feet (b), target)) {
    ++failed_boosts;
    CancelBoost (b.Index ());
    return false;
  }
  CommandToward (b, cmd, target, speed);
  if (base && p.input.helperDuck)
    cmd.buttons |= IN_DUCK;
  if (!base) {
    if (p.input.jump)
      cmd.buttons |= IN_JUMP;
    if (p.input.climberDuck)
      cmd.buttons |= IN_DUCK;
  }
  desired = (p.landing + ystl::Vector (0, 0, 30) - b.GetEyesPos ()).angles ();
  desired.x = -desired.x;
  b.IgnoreCollision ();
  return true;
}
bool HumanArena::LobbyEnabled () {
  return cv_final_lobby.ptr && cv_final_lobby.As<bool> ();
}
int HumanArena::Capacity () {
  return rbcc::finalCapacity (game.MaxClients (), bots.GetHumansCount (), cv_autovacate_keep_slots.As<int> ());
}
bool HumanArena::ReadyToJoin () {
  return !LobbyEnabled () || game.Time () >= next_join;
}
int HumanArena::LobbyTarget (int desired, int current) {
  if (!LobbyEnabled ())
    return desired;
  desired = std::clamp (desired, 0, Capacity ());
  return ReadyToJoin () ? desired : std::min (desired, current);
}
bool HumanArena::MayLeave (const Bot &b) {
  return !LobbyEnabled () || (game.Time () >= next_leave && !Boosting (b) && !b.has_progress_bar_ && !b.has_c4_);
}
void HumanArena::Leaving (Bot &b) {
  if (!LobbyEnabled ())
    return;
  CancelBoost (b.Index ());
  ++departures;
  next_join = std::max (next_join, game.Time () + Rand (3, 12));
  next_leave = game.Time () + Rand (8, 18);
}
void HumanArena::Joined (Bot &b) {
  if (ready)
    Actor (b);
  if (LobbyEnabled ()) {
    ++arrivals;
    next_join = game.Time () + Rand (cv_final_join_min.As<float> (), std::max (cv_final_join_min.As<float> (), cv_final_join_max.As<float> ()));
  }
}
void HumanArena::Status () {
  game.Print ("[FINAL 4.3.3] enabled=%d memory_profiles=%u decisions=%u repeat_fallbacks=%u boosts=%u aborted=%u lobby=%d bots=%d capacity=%d "
              "joins=%u leaves=%u",
    Enabled () ? 1 : 0, static_cast<unsigned> (memory.size ()), memory.sequence (), repeat_fallbacks, completed_boosts, failed_boosts,
    LobbyEnabled () ? 1 : 0, bots.GetBotCount (), LobbyEnabled () ? Capacity () : game.MaxClients (), arrivals, departures);
  for (const auto &b : bots) {
    const auto &a = actors[b.Index ()];
    if (PatrolMode (b))
      game.Print ("[CT Hunt] %s target_region=%d swept_tspawn=%u swept_tunnel=%u swept_pit=%u", b.pev->netname.chars (), a.patrol_zone,
        a.patrol_visits[0], a.patrol_visits[1], a.patrol_visits[2]);
    game.Print ("[FINAL] %s strategy=%s mood=%s risk=%.2f rank=%d top=%d frags=%.0f", b.pev->netname.chars (),
      rbcc::strategyName (a.persona.current), rbcc::moodName (a.persona.mood), static_cast<double> (a.persona.risk), a.persona.rank,
      a.persona.top ? 1 : 0, static_cast<double> (b.pev->frags));
  }
}
#if defined(YSTL_TESTS)
bool HumanArena::TestStartBoost (Bot &h, Bot &c, const ystl::Vector &base, const ystl::Vector &landing) {
  Actor (h);
  Actor (c);
  auto &p = pairs[static_cast<size_t> (h.team_)];
  p.helper = h.Index ();
  p.climber = c.Index ();
  p.helper_serial = h.Ent ()->serialnumber;
  p.climber_serial = c.Ent ()->serialnumber;
  p.base = base;
  p.landing = landing;
  auto d = landing - base;
  d.z = 0;
  const float length = d.length ();
  p.approach = base - d * (40.0f / std::max (length, 1.0f));
  p.started = game.Time ();
  p.machine.start (p.started, landing.z - base.z);
  return true;
}
unsigned HumanArena::TestBoosts () {
  return completed_boosts;
}
unsigned HumanArena::TestRepeats () {
  return repeat_fallbacks;
}
unsigned HumanArena::TestLeaves () {
  return departures;
}
bool HumanArena::TestBoostPoints (const Bot &b, ystl::Vector &base, ystl::Vector &approach, ystl::Vector &landing) {
  for (const auto &p : pairs)
    if (p.helper == b.Index () || p.climber == b.Index ()) {
      base = p.base;
      approach = p.approach;
      landing = p.landing;
      return true;
    }
  return false;
}
void HumanArena::TestNextJoin (float t) {
  next_join = t;
}
#endif
} // namespace bot
