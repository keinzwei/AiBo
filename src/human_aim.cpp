// FINAL 4.3.3 HUMAN AIM. GPL-2.0-or-later.
#include "copycat/copycat_aim.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <aibo.h>
namespace bot {
namespace {
struct Contact {
  ystl::Vector point {};
  uint32_t token {};
  float time { -100 }, event { -100 };
  bool heard {};
};
struct Observer {
  int serial { -1 }, user { -1 };
  uint32_t profile {}, target {};
  rbcc::Frame frame {};
  float command { -100 }, acquired {}, shot { -100 }, pending { -100 };
  uint32_t shotTarget {}, pendingTarget {};
  std::array<int, 31> clips {};
  std::array<bool, 31> known {};
  unsigned weapon {};
  uint32_t pendingVictim {};
  bool reaction {}, credited {}, pendingKill {}, pendingHead {};
};
struct Shooter {
  int serial { -1 }, user { -1 };
  uint32_t profile {}, random { 1 }, headTarget {};
  rbcc::AdaptiveAimLevel skill {};
  rbcc::ShotDiscipline discipline {};
  rbcc::AimStyle style { rbcc::AimStyle::Balanced };
  Contact contact {};
  float update {}, headUntil {}, zoomNext {}, scopedSince { -1 }, switchAt { -1 }, returnAt { -1 }, wallResume {}, lastShot { -100 }, nextHop {},
    hopAt { -100 }, practice {}, lastError { 5 }, wallWindow { -100 };
  unsigned wallShots {}, variation { 1 };
  bool head {}, wallActive {};
};
Observer observers[kGameMaxPlayers];
Shooter shooters[kGameMaxPlayers];
rbcc::AimKnowledge knowledge;
std::string map_name, data_path;
uint32_t environment {};
bool ready {};
float deathTime[kGameMaxPlayers] {};
int deathSerial[kGameMaxPlayers] {};
int Slot (const edict_t *e) {
  if (!e)
    return -1;
  const int n = game.IndexOfPlayer (e);
  return n >= 0 && n < kGameMaxPlayers ? n : -1;
}
bool RealHuman (edict_t *e) {
  return e && !e->free && (e->v.flags & FL_CLIENT) && !(e->v.flags & FL_FAKECLIENT) && !bots[e];
}
uint32_t Token (const edict_t *e) {
  return e ? (static_cast<uint32_t> (game.IndexOfEntity (e)) << 16u) ^ (static_cast<uint32_t> (e->serialnumber) & 65535u) : 0;
}
uint32_t Profile (edict_t *e, bool bot) {
  const char *name = bot ? e->v.netname.chars () : engfuncs.pfnGetPlayerAuthId (e);
  if (!name || !*name || std::strstr (name, "LAN") || std::strstr (name, "PENDING") || std::strstr (name, "UNKNOWN"))
    name = e->v.netname.chars ();
  const auto hash = rbcc::hashBytes (name, std::strlen (name)) & 0x7fffffffu;
  return (hash ? hash : 1u) | (bot ? 0x80000000u : 0u);
}
Observer &Observe (edict_t *e) {
  static Observer invalid;
  const int slot = Slot (e);
  if (slot < 0)
    return invalid;
  auto &a = observers[slot];
  const auto id = Profile (e, !RealHuman (e));
  const int user = engfuncs.pfnGetPlayerUserId (e);
  if (a.serial != e->serialnumber || a.user != user || a.profile != id) {
    a = Observer {};
    a.serial = e->serialnumber;
    a.user = user;
    a.profile = id;
  }
  return a;
}
uint32_t Random (Shooter &a) {
  a.random ^= a.random << 13u;
  a.random ^= a.random >> 17u;
  a.random ^= a.random << 5u;
  return a.random;
}
Shooter &Actor (Bot &b) {
  auto &a = shooters[b.Index ()];
  const auto id = Profile (b.Ent (), true);
  const int user = engfuncs.pfnGetPlayerUserId (b.Ent ());
  if (a.serial != b.Ent ()->serialnumber || a.user != user || a.profile != id) {
    a = Shooter {};
    a.serial = b.Ent ()->serialnumber;
    a.user = user;
    a.profile = id;
    a.random = id ^ static_cast<uint32_t> (user) ^ 0x9e3779b9u;
    a.variation = Random (a);
    std::string name = b.pev->netname.chars ();
    for (auto &c : name)
      if (c >= 'A' && c <= 'Z')
        c = static_cast<char> (c + ('a' - 'A'));
    if (human_policy::AwpProfile (b.Ent ()))
      a.style = rbcc::AimStyle::Awper;
    else if (name.find ("f0rest") != std::string::npos)
      a.style = rbcc::AimStyle::Precision;
    else if (name == "neo" || name.starts_with ("neo ("))
      a.style = rbcc::AimStyle::Spray;
    else if (name.find ("markeloff") != std::string::npos || name.find ("guardian") != std::string::npos ||
             name.find ("delpan") != std::string::npos)
      a.style = rbcc::AimStyle::Awper;
    if (const auto s = knowledge.get (id))
      a.practice = std::min (s->practiceSeconds / 3600.0f, 1.0f) * 0.5f +
                   std::min (static_cast<float> (s->tracking) / 3000.0f, 1.0f) *
                     std::min (static_cast<float> (s->hits + 1u) / static_cast<float> (s->shots + 2u) * 2.0f, 1.0f) * 0.5f;
  }
  return a;
}
bool LiveToken (const Contact &c, Team team) {
  const int index = static_cast<int> (c.token >> 16u);
  auto e = game.EntityOfIndex (index);
  return e && !e->free && (e->v.flags & FL_CLIENT) && e->v.deadflag == DEAD_NO && e->v.health > 0 && Token (e) == c.token &&
         game.GetPlayerTeam (e) != team;
}
bool TeamRay (const Bot &b, const ystl::Vector &end) {
  const auto eye = b.GetEyesPos (), line = end - eye;
  const float square = line.length_sq ();
  if (square < 1)
    return false;
  for (const auto &client : clients) {
    if (!client.IsUsedAndAlive () || client.ent == b.Ent () || !client.IsSameTeam (b.team_))
      continue;
    const auto center = client.ent->v.origin;
    const float fraction = ystl::clamp (((center - eye) | line) / square, 0.0f, 1.0f);
    if (fraction > 0.01f && fraction < 1.0f && (eye + line * fraction).distance (center) < 42)
      return false;
  }
  return true;
}
bool Penetrable (Bot &b, const ystl::Vector &end) {
  const auto eye = b.GetEyesPos ();
  const auto line = end - eye;
  const float distance = line.length ();
  if (distance < 100 || distance > 8192 || !TeamRay (b, end))
    return false;
  const auto dir = line * (1.0f / distance);
  TraceResult tr {};
  engfuncs.pfnTraceLine (eye, end, 1, b.Ent (), &tr);
  if (tr.fAllSolid || tr.fStartSolid || tr.flFraction >= 0.999f || tr.flFraction < 0.01f)
    return false;
  const auto entry = tr.vecEndPos;
  const char *texture = nullptr;
  if (engfuncs.pfnTraceTexture)
    texture = engfuncs.pfnTraceTexture (tr.pHit ? tr.pHit : game.GetStartEntity (), eye, entry);
  const char material = texture ? conf.GetMaterialType (texture) : 'C';
  const auto budget = rbcc::penetrationBudget (static_cast<unsigned> (b.current_weapon_));
  if (!budget.walls)
    return false;
  const float power = budget.power * rbcc::materialPower (material);
  bool exit = false;
  ystl::Vector cursor {};
  for (float thickness = 2; thickness <= power; thickness += 2) {
    cursor = entry + dir * thickness;
    if (engfuncs.pfnPointContents (cursor) != CONTENTS_SOLID) {
      exit = rbcc::plausibleWall (static_cast<unsigned> (b.current_weapon_), material, thickness, eye.distance (entry));
      break;
    }
  }
  if (!exit)
    return false;
  engfuncs.pfnTraceLine (cursor, end, 1, b.Ent (), &tr);
  return !tr.fAllSolid && !tr.fStartSolid && tr.flFraction >= 0.999f;
}
void CounterSteer (Bot &b, HumanCommand &cmd) {
  auto yaw = cmd.angles;
  yaw.x = yaw.z = 0;
  cmd.forward = ystl::clamp (-(b.pev->velocity | yaw.forward ()) * 1.8f, -250.0f, 250.0f);
  cmd.side = ystl::clamp (-(b.pev->velocity | yaw.right ()) * 1.8f, -250.0f, 250.0f);
  cmd.buttons &= static_cast<uint16_t> (~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT));
  if (cmd.forward > 1)
    cmd.buttons |= IN_FORWARD;
  if (cmd.forward < -1)
    cmd.buttons |= IN_BACK;
  if (cmd.side > 1)
    cmd.buttons |= IN_MOVERIGHT;
  if (cmd.side < -1)
    cmd.buttons |= IN_MOVELEFT;
}
}
bool HumanAimCoach::Enabled () {
  return ready && HumanCopycat::Enabled () && (cv_human_aim_adaptive || cv_human_aim_level.As<float> () >= 0);
}
void HumanAimCoach::Start (const char *map, uint32_t env, const char *path) {
  Stop ();
  map_name = map;
  environment = env;
  const std::string base = path;
  data_path = base.substr (0, base.size () - 5) + ".haim";
  ready = true;
  std::fill (std::begin (deathTime), std::end (deathTime), -100.0f);
  std::fill (std::begin (deathSerial), std::end (deathSerial), -1);
  std::string error;
  std::error_code ec;
  if (cv_human_aim_save && !knowledge.load (data_path, map_name, environment, error) && std::filesystem::exists (data_path, ec))
    game.Print ("[HUMAN AIM] Ignored data: %s", error.c_str ());
}
void HumanAimCoach::Stop () {
  if (ready)
    Save ();
  ready = false;
  knowledge.clear ();
  for (auto &a : observers)
    a = Observer {};
  for (auto &a : shooters)
    a = Shooter {};
}
bool HumanAimCoach::Dirty () {
  return ready && cv_human_aim_save && knowledge.dirty ();
}
bool HumanAimCoach::Save () {
  if (!Dirty ())
    return true;
  std::error_code ec;
  std::filesystem::create_directories (std::filesystem::path (data_path).parent_path (), ec);
  std::string error;
  if (ec || !knowledge.save (data_path, map_name, environment, error)) {
    game.Print ("[HUMAN AIM] Save failed: %s", ec ? ec.message ().c_str () : error.c_str ());
    return false;
  }
  return true;
}
void HumanAimCoach::ResetBot (Bot &b) {
  if (!ready)
    return;
  const auto level = Actor (b).skill;
  shooters[b.Index ()] = Shooter {};
  Actor (b).skill = level;
  observers[b.Index ()] = Observer {};
}
void HumanAimCoach::RecordHuman (edict_t *human, const rbcc::Frame &frame) {
  if (!Enabled () || !RealHuman (human) || Slot (human) < 0 || !rbcc::validFrame (frame))
    return;
  auto &a = Observe (human);
  a.frame = frame;
  a.command = game.Time ();
  if (frame.weapon)
    a.weapon = frame.weapon;
  // Several enemies can be visible. Prefer the visible player under the human's
  // actual crosshair instead of penalizing a shot at a more distant opponent.
  if (frame.context.visible && (frame.buttons & rbcc::Attack)) {
    const auto eye = human->v.origin + human->v.view_ofs;
    const ystl::Vector angle (frame.angles.x + frame.punch.x, frame.angles.y + frame.punch.y, 0);
    TraceResult sight {};
    engfuncs.pfnTraceLine (eye, eye + angle.forward () * 8192, 0, human, &sight);
    ystl::Vector point {};
    if (Slot (sight.pHit) >= 0 && game.GetPlayerTeam (sight.pHit) != game.GetPlayerTeam (human) &&
        HumanCopycat::VisualPoint (human, sight.pHit, point)) {
      const auto look = (point - eye).angles ();
      a.frame.context.targetToken = Token (sight.pHit);
      a.frame.context.enemy = rbcc::Vec (point.x, point.y, point.z);
      a.frame.context.enemyDistance = point.distance (eye);
      a.frame.context.enemyYaw = rbcc::angleDelta (look.y, frame.angles.y);
      a.frame.context.enemyPitch = rbcc::angleDelta (-look.x, frame.angles.x);
    }
  }
  if (a.target != a.frame.context.targetToken) {
    a.target = a.frame.context.targetToken;
    a.acquired = game.Time ();
    a.reaction = false;
  }
}
void HumanAimCoach::WeaponUpdate (edict_t *e, int active, unsigned w, int clip) {
  if (!Enabled () || Slot (e) < 0 || !e || e->free || w >= 31 || w == 0 || clip < 0 || clip > 100 || !(e->v.flags & FL_CLIENT))
    return;
  auto &a = Observe (e);
  if (active)
    a.weapon = w;
  const int previous = a.clips[w];
  const bool shot = active && a.known[w] && previous > clip && previous - clip <= 6;
  a.clips[w] = clip;
  a.known[w] = true;
  if (!shot || rbcc::isUtility (w) || w == 6 || w == 29)
    return;
  const unsigned count = static_cast<unsigned> (previous - clip);
  const float now = game.Time ();
  if (auto b = bots[e]) {
    auto &s = Actor (*b);
    s.discipline.confirmed (now, w, count);
    s.variation = Random (s);
    s.lastShot = now;
    if (w == 18 && s.skill.value > 0.4f && cv_human_aim_quickswitch)
      s.switchAt = now + 0.13f;
    if (s.wallActive) {
      s.wallShots += count;
      if (s.wallShots >= 4)
        s.wallResume = now + 0.8f;
    }
    if (auto p = knowledge.edit (s.profile))
      for (unsigned i = 0; i < count; ++i)
        p->shot (s.lastError, (e->v.flags & FL_ONGROUND) && e->v.velocity.length2d () < 45);
    a.shot = now;
    a.credited = false;
    a.shotTarget = s.contact.token;
  }
  else if (RealHuman (e) && now - a.command < 0.25f && a.frame.context.visible && a.frame.context.targetToken) {
    if (auto p = knowledge.edit (a.profile)) {
      const float error = std::hypot (a.frame.context.enemyYaw - a.frame.punch.y, a.frame.context.enemyPitch - a.frame.punch.x);
      for (unsigned i = 0; i < count; ++i)
        p->shot (error, (a.frame.state & rbcc::Ground) && a.frame.velocity.length () < 45);
      if (!a.reaction) {
        p->reaction ((now - a.acquired) * 1000);
        a.reaction = true;
      }
      a.shot = now;
      a.shotTarget = a.frame.context.targetToken;
      a.credited = false;
    }
  }
  if (a.pendingTarget == a.shotTarget && now - a.pending < 0.2f && !a.credited) {
    if (auto p = knowledge.edit (a.profile))
      p->hit ();
    a.credited = true;
  }
  if (a.pendingKill && a.pendingTarget == a.shotTarget && now - a.pending < 0.2f) {
    if (auto p = knowledge.edit (a.profile))
      p->duel (true, a.pendingHead);
    if (auto p = knowledge.edit (a.pendingVictim))
      p->duel (false);
    a.pendingKill = false;
  }
}
void HumanAimCoach::Damage (edict_t *victim, edict_t *attacker, int damage, int bits) {
  if (!Enabled () || !(bits & (1 << 1)) || damage <= 0 || Slot (victim) < 0 || Slot (attacker) < 0 || victim == attacker ||
      game.GetPlayerTeam (victim) == game.GetPlayerTeam (attacker))
    return;
  auto &a = Observe (attacker);
  const float now = game.Time ();
  const auto token = Token (victim);
  if (now - a.shot < 0.7f && a.shotTarget == token && !a.credited) {
    if (auto p = knowledge.edit (a.profile))
      p->hit ();
    a.credited = true;
  }
  else if (now - a.shot > 0.02f) {
    a.pending = now;
    a.pendingTarget = token;
  }
}
void HumanAimCoach::Death (edict_t *killer, edict_t *victim, bool head) {
  if (!Enabled () || Slot (victim) < 0 || Slot (killer) < 0 || killer == victim || game.GetPlayerTeam (killer) == game.GetPlayerTeam (victim))
    return;
  const int v = Slot (victim);
  if (deathSerial[v] == victim->serialnumber && game.Time () - deathTime[v] < 0.1f)
    return;
  deathSerial[v] = victim->serialnumber;
  deathTime[v] = game.Time ();
  auto &a = Observe (killer);
  if (game.Time () - a.shot < 0.7f && a.shotTarget == Token (victim)) {
    if (auto p = knowledge.edit (a.profile))
      p->duel (true, head);
    if (auto p = knowledge.edit (Observe (victim).profile))
      p->duel (false);
  }
  else if (a.weapon != 0 && a.weapon != 6 && a.weapon != 29 && !rbcc::isUtility (a.weapon)) {
    a.pending = game.Time ();
    a.pendingTarget = Token (victim);
    a.pendingKill = true;
    a.pendingHead = head;
    a.pendingVictim = Observe (victim).profile;
  }
}
float HumanAimCoach::Update (Bot &b, edict_t *opponent) {
  if (!Enabled ())
    return 0;
  auto &a = Actor (b);
  const float now = game.Time ();
  const float manual = cv_human_aim_level.As<float> ();
  if (manual >= 0) {
    a.skill.value = std::clamp (manual * 0.01f, 0.0f, 1.0f);
    return Level (b);
  }
  if (now < a.update)
    return Level (b);
  const float dt = a.update > 0 ? std::clamp (now - a.update + 0.5f, 0.0f, 2.0f) : 0.5f;
  a.update = now + 0.5f;
  float sum = 0, weight = 0, confidence = 0;
  bool activeHuman = false;
  auto add = [&] (edict_t *e, bool includeTeam) {
    if (!RealHuman (e))
      return;
    const auto team = game.GetPlayerTeam (e);
    if (team != Team::CT && team != Team::Terrorist)
      return;
    activeHuman = true;
    if (!includeTeam && team == b.team_)
      return;
    const auto p = knowledge.get (Profile (e, false));
    if (!p)
      return;
    const float c = p->confidence ();
    if (c <= 0)
      return;
    sum += p->estimate () * c;
    weight += c;
    confidence = std::max (confidence, c);
  };
  if (RealHuman (opponent))
    add (opponent, false);
  if (weight <= 0)
    for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i)
      add (game.PlayerOfIndex (i), false);
  if (weight <= 0)
    for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i)
      add (game.PlayerOfIndex (i), true);
  if (!activeHuman)
    a.skill.update (dt, a.practice, 1);
  else
    a.skill.update (dt, weight > 0 ? sum / weight : 0, confidence);
  return Level (b);
}
float HumanAimCoach::Level (const Bot &b) {
  return Enabled () ? shooters[b.Index ()].skill.value * human_policy::RankPower (b.Ent ()) : 0;
}
float HumanAimCoach::Practice (const Bot &b) {
  return Enabled () ? shooters[b.Index ()].practice : 0;
}
bool HumanAimCoach::PreferHead (Bot &b, uint32_t token) {
  auto &a = Actor (b);
  if (!Enabled () || Level (b) <= 0 || !cv_human_aim_techniques)
    return false;
  if (a.headTarget != token || game.Time () > a.headUntil) {
    a.headTarget = token;
    a.headUntil = game.Time () + 0.8f;
    const float chance = (a.style == rbcc::AimStyle::Precision ? 0.95f : 0.82f) * Level (b);
    a.head = static_cast<float> (Random (a) % 1000u) < chance * 1000;
  }
  return a.head;
}
void HumanAimCoach::Seen (Bot &b, uint32_t token, const ystl::Vector &point) {
  if (!Enabled ())
    return;
  auto &a = Actor (b);
  a.contact = { point, token, game.Time (), game.Time (), false };
}
void HumanAimCoach::Heard (Bot &b, edict_t *source, const ystl::Vector &point, float started) {
  if (!Enabled () || Slot (source) < 0 || game.Time () - started > 1.0f || started > game.Time ())
    return;
  auto &a = Actor (b);
  if (a.contact.heard && a.contact.token == Token (source) && a.contact.event >= started)
    return;
  if (!a.contact.heard && game.Time () - a.contact.time < 0.25f)
    return;
  const float error = std::clamp (point.distance (b.pev->origin) * 0.04f, 12.0f, 100.0f);
  auto guess = point;
  guess.x += (static_cast<float> (Random (a) % 2001u) * 0.001f - 1) * error;
  guess.y += (static_cast<float> (Random (a) % 2001u) * 0.001f - 1) * error;
  guess.z += 8;
  a.contact = { guess, Token (source), started, started, true };
}
bool HumanAimCoach::Wallbang (Bot &b, ystl::Vector &point, uint32_t &token) {
  auto &a = Actor (b);
  a.wallActive = false;
  if (!Enabled () || !cv_human_aim_wallbang || Level (b) < 0.35f || !cv_human_aim_techniques || game.Time () < a.wallResume ||
      !LiveToken (a.contact, b.team_))
    return false;
  const float age = game.Time () - a.contact.time;
  if (age < 0 || age > (a.contact.heard ? 0.75f : 1.1f))
    return false;
  if (game.Time () - a.wallWindow > 1.5f) {
    a.wallWindow = game.Time ();
    a.wallShots = 0;
  }
  if (a.wallShots >= 4 || !Penetrable (b, a.contact.point))
    return false;
  point = a.contact.point;
  token = a.contact.token;
  a.wallActive = true;
  return true;
}
void HumanAimCoach::Prepare (Bot &b, HumanCommand &cmd, ystl::Vector &desired, bool visible, bool wall, bool available, bool motor) {
  if (!Enabled ())
    return;
  auto &a = Actor (b);
  const float level = Level (b), now = game.Time ();
  if (!available || level <= 0 || !cv_human_aim_techniques) {
    a.switchAt = a.returnAt = -1;
    a.wallActive = false;
    return;
  }
  if (a.switchAt >= 0 && now >= a.switchAt) {
    a.switchAt = -1;
    if (cv_human_aim_quickswitch && b.current_weapon_ == Weapon::AWP && b.GetAmmoInClip () > 0 &&
        (!visible || b.pev->origin.distance (a.contact.point) > 350)) {
      Weapon side = Weapon::Knife;
      for (auto w : { Weapon::Deagle, Weapon::USP, Weapon::Glock18 })
        if (b.pev->weapons & (1 << static_cast<int> (w))) {
          side = w;
          break;
        }
      b.SelectWeaponById (side);
      a.returnAt = now + 0.18f;
    }
  }
  if (a.returnAt >= 0 && now >= a.returnAt) {
    a.returnAt = -1;
    if (b.pev->weapons & (1 << static_cast<int> (Weapon::AWP)))
      b.SelectWeaponById (Weapon::AWP);
  }
  if (b.current_weapon_ == Weapon::AWP && (visible || wall)) {
    if (b.pev->fov >= 89 || b.pev->fov <= 1) {
      a.scopedSince = -1;
      cmd.buttons &= static_cast<uint16_t> (~IN_ATTACK2);
      if (now >= a.zoomNext) {
        cmd.buttons |= IN_ATTACK2;
        a.zoomNext = now + 0.35f;
      }
    }
    else {
      cmd.buttons &= static_cast<uint16_t> (~IN_ATTACK2);
      if (a.scopedSince < 0)
        a.scopedSince = now;
    }
  }
  if (!visible && !wall && !motor && cv_human_aim_preaim && !HumanArena::PatrolActive (b) && b.GetTaskId () != TaskId::Camp &&
      b.GetTaskId () != TaskId::Hide) {
    ystl::Vector point {};
    if (a.contact.token && now - a.contact.time < 1.5f)
      point = a.contact.point;
    else if (!b.look_at_.empty () && std::fabs (b.look_at_.z - b.pev->origin.z) < 80) {
      point = b.look_at_;
      point.z = b.pev->origin.z + 23;
    }
    if (!point.empty ()) {
      const auto angle = (point - b.GetEyesPos ()).angles ();
      desired.x = desired.x + (-angle.x - desired.x) * level;
      desired.y = angle.y;
    }
  }
  if (!visible && !wall && !motor && level > 0.65f && cv_human_aim_crouchhop && cv_human_movement && has_flag (b.states_, Sense::HearingEnemy) &&
      !b.has_c4_ && !b.is_vip_ && !b.has_hostage_ && !b.IsOnLadder () && !b.IsInWater () && b.GetTaskId () == TaskId::Normal) {
    if (b.IsOnFloor () && now >= a.nextHop && b.pev->velocity.length2d () > 120 && !(cmd.buttons & (IN_USE | IN_JUMP))) {
      auto angle = cmd.angles;
      angle.x = angle.z = 0;
      const auto foot = b.pev->origin - ystl::Vector (0, 0, 36);
      const auto ahead = foot + angle.forward () * 48;
      TraceResult arc {};
      engfuncs.pfnTraceHull (b.pev->origin, b.pev->origin + ystl::Vector (0, 0, 56), 1, human_hull, b.Ent (), &arc);
      if (!arc.fAllSolid && !arc.fStartSolid && arc.flFraction >= 0.999f && HumanExplore::Walkable (b, foot, ahead)) {
        cmd.buttons |= IN_JUMP;
        a.hopAt = now;
        a.nextHop = now + 1.5f + static_cast<float> (Random (a) % 100u) * 0.01f;
      }
    }
    if (!b.IsOnFloor () && now - a.hopAt > 0.04f && now - a.hopAt < 0.32f)
      cmd.buttons |= IN_DUCK;
  }
}
bool HumanAimCoach::Finish (Bot &b, HumanCommand &cmd, bool visible, bool wall, bool available, bool allows, float distance) {
  if (!Enabled ())
    return allows;
  auto &a = Actor (b);
  const unsigned w = static_cast<unsigned> (b.current_weapon_);
  const auto target = (visible || wall) ? a.contact.token : 0u;
  if (available && visible) {
    if (auto p = knowledge.edit (a.profile)) {
      p->practiceSeconds = std::min (10000000.0f, p->practiceSeconds + static_cast<float> (cmd.msec) * 0.001f);
      a.practice = std::min (p->practiceSeconds / 3600.0f, 1.0f) * 0.5f +
                   std::min (static_cast<float> (p->tracking) / 3000.0f, 1.0f) *
                     std::min (static_cast<float> (p->hits + 1u) / static_cast<float> (p->shots + 2u) * 2, 1.0f) * 0.5f;
    }
  }
  const auto wish = (a.contact.point - b.GetEyesPos ()).angles ();
  a.lastError =
    std::hypot (rbcc::angleDelta (-wish.x, cmd.angles.x + b.pev->punchangle.x), rbcc::angleDelta (wish.y, cmd.angles.y + b.pev->punchangle.y));
  if (Level (b) <= 0 || !cv_human_aim_techniques)
    return allows;
  const auto plan = rbcc::shootingPlan (w, distance, Level (b), a.style, a.variation);
  if (available && (visible || wall) && b.IsOnFloor () && b.pev->velocity.length2d () > plan.maxSpeed && !b.IsOnLadder () && !b.IsInWater () &&
      cv_human_movement)
    CounterSteer (b, cmd);
  const bool scope =
    b.pev->fov > 1 && b.pev->fov < 89 && (a.scopedSince >= 0 && game.Time () - a.scopedSince >= 0.04f + (1 - Level (b)) * 0.08f);
  bool fire =
    a.discipline.allow (game.Time (), target, w, available && allows, b.IsOnFloor (), scope, b.pev->velocity.length2d (), a.lastError, plan);
  if (wall && (!TeamRay (b, a.contact.point) || !Penetrable (b, a.contact.point)))
    fire = false;
  if (a.returnAt >= 0 || (a.switchAt >= 0 && game.Time () >= a.switchAt))
    fire = false;
  if (fire)
    cmd.buttons |= IN_ATTACK;
  else
    cmd.buttons &= static_cast<uint16_t> (~IN_ATTACK);
  return fire;
}
void HumanAimCoach::Status () {
  game.Print ("[HUMAN AIM 4.3.3] enabled=%d manual=%.0f profiles=%u baseline=4.3.0; aim is fallible, engine spread/cooldown unchanged",
    Enabled () ? 1 : 0, static_cast<double> (cv_human_aim_level.As<float> ()), static_cast<unsigned> (knowledge.size ()));
  for (const auto &b : bots) {
    const auto &a = shooters[b.Index ()];
    const auto pro = rbcc::proProfile (b.pev->netname.chars ());
    game.Print ("[Pro profile] %s role=%s hltv_peak=%d year=%d strength=%.3f", b.pev->netname.chars (),
      pro && pro->weapon == rbcc::ProWeapon::AWP ? "AWP"
      : pro                                      ? "rifle"
                                                 : "generic",
      pro ? pro->rank : 0, pro ? pro->year : 0, static_cast<double> (human_policy::RankPower (b.Ent ())));
    game.Print ("[HUMAN AIM] %s level=%.1f%% practice=%.2f mode=%u confirmed_burst=%u", b.pev->netname.chars (),
      static_cast<double> (Level (b) * 100), static_cast<double> (a.practice), static_cast<unsigned> (a.discipline.plan ().mode),
      a.discipline.fired ());
  }
  for (int i = 0; i < std::min (game.MaxClients (), kGameMaxPlayers); ++i) {
    auto e = game.PlayerOfIndex (i);
    if (!RealHuman (e))
      continue;
    if (const auto p = knowledge.get (Profile (e, false)))
      game.Print ("[HUMAN AIM] human %s shots=%u hits=%u confidence=%.2f estimated=%.2f", e->v.netname.chars (), p->shots, p->hits,
        static_cast<double> (p->confidence ()), static_cast<double> (p->estimate ()));
  }
}
#if defined(YSTL_TESTS)
unsigned HumanAimCoach::TestShots (edict_t *e) {
  const auto p = knowledge.get (Profile (e, false));
  return p ? p->shots : 0;
}
unsigned HumanAimCoach::TestHits (edict_t *e) {
  const auto p = knowledge.get (Profile (e, false));
  return p ? p->hits : 0;
}
unsigned HumanAimCoach::TestHeadKills (edict_t *e) {
  const auto p = knowledge.get (Profile (e, false));
  return p ? p->headKills : 0;
}
float HumanAimCoach::TestEstimate (edict_t *e) {
  const auto p = knowledge.get (Profile (e, false));
  return p ? p->estimate () : 0;
}
ystl::Vector HumanAimCoach::TestContact (const Bot &b) {
  return shooters[b.Index ()].contact.point;
}
#endif
}
