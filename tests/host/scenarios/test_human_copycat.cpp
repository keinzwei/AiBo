// GPL-2.0-or-later. Synthetic integration tests, not a CS1.6 match benchmark.
#include "../../../src/copycat/copycat_core.h"
#include <aibo.h>
#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"
#include <filesystem>
#include <fstream>
#include <set>

extern plugin_info_t Plugin_info;

namespace bot {
struct HumanCopycatHook {
  static ystl::RWrand &Rng (Bot &b) { return b.rg; }
  static bool Restricted (Bot &b, Weapon id) { return b.IsWeaponRestricted (id); }
  static void Select (Bot &b, Weapon id) { b.SelectWeaponById (id); }
  static Weapon Primary (Bot &b) {
    const auto selected = b.SelectBuyPrimary (conf.GetWeaponPrefs (b.personality_), conf.GetWeapons (), 400);
    return selected ? selected->id : Weapon::Invalid;
  }
  static void Secondary (Bot &b) { b.BuySecondaryWeapon (conf.GetWeaponPrefs (b.personality_), conf.GetWeapons ()); }
  static void Buy (Bot &b, const WeaponInfo *w) { b.IssueBuyCommand (w); }
  static void BuyContextSetup (Bot &b) { b.buy_context_ = game.Is (GameFlags::Legacy) ? BuyContext::IsLegacy : BuyContext::None; }
  static void BuyNades (Bot &b) { b.BuyGrenades (); }
  static Weapon BestNade (Bot &b) { return b.GetBestGrenadeCarriedId (); }
  static void Nades (Bot &b, const ystl::Vector &point) {
    b.last_enemy_origin_ = point; b.states_ = Sense::SuspectEnemy; b.fear_level_ = 0.0f;
    b.is_using_grenade_ = false; b.reload_data_.Clear (); b.grenade_check_timer_.invalidate ();
    b.CheckGrenadesThrow ();
  }
  static bool NadeSafe (Bot &b, Weapon id, const ystl::Vector &point) { return b.IsGrenadeTargetSafe (id, point); }
  static void Chat (Bot &b) { b.CheckForChat (); }
  static ystl::String Pending (Bot &b) { return b.pending_generated_chat_; }
  static bool PendingTeam (Bot &b) { return b.pending_generated_team_; }
  static void NoChatCooldown (Bot &b) { b.pending_generated_chat_.clear (); b.next_generated_chat_ = 0.0f; }
  static void States (Bot &b, Sense sense) { b.states_ = sense; }
  static void OpenMovement (Bot &b) {
    b.stuck_timer_.invalidate (); b.path_flags_ = NodeFlag {}; b.jump_knife_drawn_ = false;
    b.is_using_grenade_ = false; b.reload_data_.Clear (); b.has_progress_bar_ = false;
    b.SetMoveAllowed (true); b.pev->movetype = MOVETYPE_WALK; b.pev->waterlevel = 0;
  }
  static void Setup (Bot &b) {
    b.is_alive_ = true; b.team_ = Team::Terrorist; b.buying_finished_ = true;
    b.not_started_ = false; b.current_weapon_ = Weapon::AK47; b.weapon_type_ = WeaponType::Rifle;
    b.move_angles_.clear (); b.move_speed_ = 100.0f; b.strafe_speed_ = 0.0f;
    b.blind_timer_.invalidate (); b.ClearTasks ();
    b.StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  }
  static void Enemy (Bot &b, edict_t *e) { b.enemy_ = e; b.aim_flags_ = e ? AimFlags::Enemy : AimFlags::Nav; b.wants_to_fire_ = e != nullptr; }
  static edict_t *Enemy (Bot &b) { return b.enemy_; }
  static void Look (Bot &b, const ystl::Vector &p) { b.look_at_ = p; }
  static void UpdateLook (Bot &b) { b.UpdateLookAngles (); }
  static void Memory (Bot &b, edict_t *e, const ystl::Vector &p) { b.last_enemy_ = e; b.last_enemy_origin_ = p; b.tracking_edict_ = e; b.next_tracking_timer_.invalidate (); }
  static ystl::Vector Memory (Bot &b) { return b.last_enemy_origin_; }
  static void Hear (Bot &b) { b.UpdateHearing (); }
  static void Damage (Bot &b, edict_t *e) { b.TakeDamage (e, 10, 0, 0); }
  static void Objective (Bot &b, bool enabled) { b.has_progress_bar_ = enabled; }
  static void Grenade (Bot &b, const ystl::Vector &velocity) { b.grenade_ = velocity; }
  static edict_t *CorrectGrenade (Bot &b) { return b.SetCorrectGrenadeVelocity (kExplosiveModelName); }
  static void LastCommand (Bot &b, float time) { b.last_command_time_ = time; }
  static void CurrentNode (Bot &b, int index) { b.current_node_index_ = index; }
  static float Score (Bot &b, edict_t *enemy) { return b.EngagementScore (enemy); }
  static void CampTask (Bot &b) { b.StartTask (TaskId::Camp,TaskPri::kCamp,kInvalidNodeIndex,game.Time ()+30,true); }
  static void RunCamp (Bot &b) { b.TaskCamp (); }
  static TaskId TaskIdOf (Bot &b) { return b.GetTaskId (); }
  static int GoalData (Bot &b) { return b.Task ()->data; }
  static void BombTask (Bot &b, bool defuse) {
    b.StartTask (defuse ? TaskId::DefuseBomb : TaskId::PlantBomb,defuse ? TaskPri::kDefuseBomb : TaskPri::kPlantBomb,kInvalidNodeIndex,0,true);
  }
  static void Plant (Bot &b) { b.TaskPlantBomb (); }
  static void Defuse (Bot &b) { b.TaskDefuseBomb (); }
  static void Smoke (Bot &b) { b.TaskThrowSmoke (); }
  static void Complete (Bot &b) { b.CompleteTask (); }
  static ystl::Vector ThrowPoint (Bot &b) { return b.throw_; }
  static void Pickup (Bot &b, edict_t *e) { b.pickup_item_ = e; }
};
namespace {
ystl::Array<HumanCommand> emitted;
void Capture (edict_t *, const float *angles, float forward, float side, float up, uint16_t buttons, uint8_t impulse, uint8_t msec) {
  emitted.push ({ystl::Vector (angles[0], angles[1], angles[2]), forward, side, up, buttons, impulse, msec});
}
struct Fixture {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};
  gamefuncs_t table {};
  Bot *bot {};
  bool Start (const char *map, bool bomb_map = false, int slots = 16) {
    engine.Initialise (AIBO_TEST_GAMEDIR "/cstrike",slots);
    engine.Globals ().mapname = string_t (engine.AllocString (map));
    if (!testhost::ResolveFakeCs (AIBO_TEST_CSDLL, cs)) return false;
    cs.reset (); GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
    if (!GetEntityAPI (&table, 140)) return false;
    game.Precache (); if (bomb_map) engine.SpawnEntity ("func_bomb_target");
    table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), slots);
    // FakeEngine::AllocEdict does not reserve client indices. Put map spawns
    // outside them, matching real GoldSrc's entity allocation contract.
    if (slots == 32) for (int i = 0; i < 32; ++i) {
      auto e = engine.EdictList ()+33+i; *e = edict_t {}; e->v.pContainingEntity = e;
      e->v.classname = string_t (engine.AllocString (i%2 ? "info_player_start" : "info_player_deathmatch"));
      game.OnSpawnEntity (e);
    }
    analyzer.Suspend (); graph.Reset ();
    for (int i = 0; i < 4; ++i) {
      Path p {}; p.origin = {100.0f * static_cast<float> (i),0.0f,0.0f}; p.number = i;
      for (auto &l : p.links) l.index = kInvalidNodeIndex;
      graph.paths_.push (p);
    }
    graph.PopulateNodes (); planner.Init ();
    cv_quota.Set (10); cv_human_copycat.Set (1); cv_human_replay.Set (0); cv_human_record.Set (1);
    cv_whose_your_daddy.Set (1); cv_shoots_thru_walls.Set (4); // hostile configuration must not bypass safeguards
    bots.InitQuota (); bots.Addbot ("HumanTest", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
    if (!testhost::PumpBots (engine, 1)) return false;
    bot = testhost::FindBot ("HumanTest"); if (!bot) return false;
    HumanCopycatHook::Setup (*bot);
    bot->pev->flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
    bot->pev->origin.clear (); bot->pev->view_ofs = {0.0f,0.0f,17.0f}; bot->pev->v_angle.clear ();
    bot->pev->maxspeed = 250.0f; bot->pev->fov = 90.0f; bot->pev->health = 100.0f;
    bot->pev->deadflag = DEAD_NO; bot->pev->viewmodel = string_t (engine.AllocString ("models/v_ak47.mdl"));
    bot->pev->weapons = 1 << 28;
    clients.Update (); clients[bot->Ent ()].team = clients[bot->Ent ()].team2 = Team::Terrorist;
    engfuncs.pfnRunPlayerMove = &Capture; emitted.clear ();
    return true;
  }
  edict_t *Foe (const ystl::Vector &pos = {500.0f,0.0f,0.0f}) {
    auto e = game.CreateFakeClient ("Teacher");
    e->v.flags = FL_CLIENT | FL_ONGROUND; e->v.health = 100.0f; e->v.deadflag = DEAD_NO;
    e->v.solid = SOLID_BBOX; e->v.origin = pos; e->v.view_ofs = {0.0f,0.0f,17.0f};
    e->v.maxspeed = 250.0f; e->v.fov = 90.0f; e->v.viewmodel = string_t (engine.AllocString ("models/v_ak47.mdl"));
    e->v.weapons = 1 << 28;
    clients.Update (); clients[e].team = clients[e].team2 = Team::CT;
    clients[bot->Ent ()].team = clients[bot->Ent ()].team2 = Team::Terrorist;
    return e;
  }
  void Wall (bool closed) {
    engine.SetTraceLineHook ([closed] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
      testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
      if (closed) { tr->flFraction = 0.5f; tr->pHit = nullptr; }
    });
  }
  ~Fixture () {
    if (bot) { HumanCopycat::LevelStop (); game.LevelShutdown (); bots.Destroy (); }
  }
};
HumanCommand MakeCommand (uint16_t buttons = 0) { return {{0.0f,0.0f,0.0f},100.0f,0.0f,0.0f,buttons,0,25}; }
rbcc::Clip MovementClip (Bot &b) {
  rbcc::Clip clip; clip.profile = 88;
  for (unsigned i = 0; i < 30; ++i) {
    rbcc::Frame f; f.msec = 10; f.weapon = 28; f.state = rbcc::Ground; f.maxSpeed = 250;
    f.context.team = 0; f.context.phase = 0; f.origin = {0,0,0}; f.forward = 100;
    f.buttons = (i == 1 || i == 3) ? rbcc::Duck : 0;
    f.previousButtons = i ? clip.frames.back ().buttons : 0;
    clip.frames.push_back (f); clip.durationMs += f.msec;
  }
  (void)b; clip.features = rbcc::analyze (clip.frames); clip.end = {0,0,0};
  return clip;
}
}

TEST_CASE ("human/visibility") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_visibility")); auto e = f.Foe (); ystl::Vector point {};
  CHECK (HumanCopycat::VisualPoint (f.bot->Ent (), e, point));
  CHECK (point.z == Approx (10.0f)); // torso, not mandatory head locking
  f.Wall (true); CHECK (!HumanCopycat::VisualPoint (f.bot->Ent (), e, point));
  f.Wall (false); e->v.origin = {-500.0f,0.0f,0.0f}; CHECK (!HumanCopycat::VisualPoint (f.bot->Ent (), e, point));
  e->v.origin = {500.0f,100.0f,0.0f}; f.bot->pev->fov = 10.0f;
  CHECK (!HumanCopycat::VisualPoint (f.bot->Ent (), e, point));
  f.bot->pev->fov = 90; HumanCopycat::Blind (f.bot->Ent (), 2.0f);
  CHECK (!HumanCopycat::VisualPoint (f.bot->Ent (), e, point));
}
TEST_CASE ("human/reaction_and_occlusion") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_reaction")); auto e = f.Foe ({500.0f,150.0f,0.0f});
  HumanCopycatHook::Enemy (*f.bot,e); HumanCopycatHook::Look (*f.bot,e->v.origin);
  bool fired = false;
  for (int i = 0; i < 45; ++i) {
    usercmd_t reentrant {}; reentrant.msec = 25;
    HumanCopycat::Record (f.bot->Ent (), &reentrant); // RunPlayerMove can re-enter CmdStart
    auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot,cmd);
    if (i < 4) CHECK ((cmd.buttons & IN_ATTACK) == 0);
    CHECK (std::fabs (cmd.angles.y) < 90.0f);
    fired = fired || (cmd.buttons & IN_ATTACK); f.engine.AdvanceTime (0.025f);
  }
  CHECK (fired);
  // A door can close after think; even a stale enemy pointer cannot fire through it.
  f.Wall (true); auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot,cmd);
  CHECK ((cmd.buttons & IN_ATTACK) == 0);
  HumanCopycat::BeforeThink (*f.bot); CHECK (HumanCopycatHook::Enemy (*f.bot) == nullptr);
}
TEST_CASE ("human/final_authority_and_movement") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_authority")); HumanCopycatHook::Look (*f.bot,{0.0f,500.0f,17.0f});
  HumanCopycatHook::UpdateLook (*f.bot); CHECK (f.bot->pev->v_angle.y == Approx (0.0f));
  auto cmd = MakeCommand (); HumanCopycat::Finalize (*f.bot,cmd);
  CHECK (cmd.angles.y > 0.0f); CHECK (cmd.angles.y < 5.0f);
  const auto world = cmd.angles.forward () * cmd.forward + cmd.angles.right () * cmd.side;
  CHECK (world.x == Approx (100.0f).margin (0.1)); CHECK (world.y == Approx (0.0f).margin (0.1));
  auto before = f.bot->pev->velocity; HumanCopycat::Finalize (*f.bot,cmd);
  CHECK (f.bot->pev->velocity.distance (before) < 0.001f);
}
TEST_CASE ("human/memory_and_hearing") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_memory")); auto e = f.Foe ();
  const ystl::Vector remembered {400.0f,80.0f,0.0f}; HumanCopycatHook::Memory (*f.bot,e,remembered);
  f.Wall (true); e->v.origin = {950.0f,700.0f,0.0f}; HumanCopycatHook::Hear (*f.bot);
  CHECK (HumanCopycatHook::Memory (*f.bot).distance (remembered) < 0.01f);
  HumanCopycatHook::Damage (*f.bot,e); CHECK (HumanCopycatHook::Memory (*f.bot).distance (remembered) < 0.01f);
  auto &noise = clients[e].noise; noise.pos = {300.0f,20.0f,0.0f}; noise.last = game.Time () + 1.0f;
  noise.start = game.Time (); noise.dist = 2000.0f; noise.type = Noise::WeaponFire;
  HumanCopycatHook::Hear (*f.bot);
  CHECK (HumanCopycatHook::Memory (*f.bot).distance (noise.pos) < 100.0f);
  CHECK (HumanCopycatHook::Memory (*f.bot).distance (e->v.origin) > 400.0f);
}
TEST_CASE ("human/team_contacts") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_contacts")); auto e = f.Foe (); edict_t *contact = nullptr; ystl::Vector p {};
  f.Wall (true); bots.ReportTeamContact (Team::Terrorist,e,e->v.origin,1,f.bot->Ent ());
  CHECK (!bots.GetTeamContact (Team::Terrorist,contact,p,6.0f));
  f.Wall (false); const auto seen = e->v.origin; bots.ReportTeamContact (Team::Terrorist,e,seen,1,f.bot->Ent ());
  CHECK (!bots.GetTeamContact (Team::Terrorist,contact,p,6.0f)); // communication latency
  f.engine.AdvanceTime (0.5f); f.Wall (true); e->v.origin = {900.0f,900.0f,0.0f};
  CHECK (bots.GetTeamContact (Team::Terrorist,contact,p,6.0f));
  CHECK (p.distance (seen) < 60.0f); CHECK (p.distance (e->v.origin) > 500.0f);
  ++e->serialnumber; CHECK (!bots.GetTeamContact (Team::Terrorist,contact,p,6.0f));
}
TEST_CASE ("human/smoke_and_grenade_physics") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_smoke")); auto e = f.Foe (); ystl::Vector point {};
  auto g = f.engine.SpawnEntity ("grenade"); g->v.origin = {240.0f,0.0f,0.0f};
  g->v.flags = FL_ONGROUND; g->v.model = string_t (f.engine.AllocString ("models/w_smokegrenade.mdl"));
  HumanCopycat::Frame (); CHECK (!HumanCopycat::VisualPoint (f.bot->Ent (),e,point));
  g->v.model = string_t (f.engine.AllocString ("models/w_hegrenade.mdl")); g->v.owner = f.bot->Ent ();
  g->v.velocity = {100.0f,20.0f,50.0f}; HumanCopycatHook::Grenade (*f.bot,{900.0f,0.0f,400.0f});
  CHECK (HumanCopycatHook::CorrectGrenade (*f.bot) == g);
  CHECK (g->v.velocity.x == Approx (100.0f)); CHECK (g->v.velocity.z == Approx (50.0f));
}
TEST_CASE ("human/replay_edges_and_objectives") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_edges"));
  auto clip = MovementClip (*f.bot); HOST_REQUIRE (HumanCopycat::TestAddClip (clip) != 0);
  cv_human_replay.Set (1); f.engine.Globals ().time = game_state.GetRoundStartTime () + 1.0f;
  HumanCopycatHook::Look (*f.bot,{500.0f,0.0f,17.0f});
  HumanCopycatHook::LastCommand (*f.bot,game.Time () - 0.05f); tickmgr.RunCommand (f.bot);
  unsigned elapsed = 0, duck_edges = 0; bool last = false;
  for (const auto &cmd : emitted) {
    elapsed += cmd.msec; const bool duck = (cmd.buttons & IN_DUCK) != 0;
    if (duck && !last) ++duck_edges;
    last = duck;
  }
  CHECK (elapsed == 50); CHECK (duck_edges == 2); CHECK (HumanCopycat::TestReplaying (*f.bot));
  HumanCopycatHook::Objective (*f.bot,true); auto cmd = MakeCommand (IN_USE); HumanCopycat::Finalize (*f.bot,cmd);
  CHECK (!HumanCopycat::TestReplaying (*f.bot)); CHECK ((cmd.buttons & IN_USE) != 0);
}
TEST_CASE ("human/recording_and_roundtrip") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_recording")); auto teacher = f.Foe ();
  cv_human_teacher.Set (0); const auto before = HumanCopycat::TestClipCount ();
  usercmd_t cmd {}; cmd.msec = 10; cmd.forwardmove = 50; cmd.viewangles.clear ();
  teacher->v.v_angle.clear ();
  for (int i = 0; i < 280; ++i) { HumanCopycat::Record (teacher,&cmd); f.engine.AdvanceTime (0.01f); }
  CHECK (HumanCopycat::TestClipCount () > before);
  const auto count = HumanCopycat::TestClipCount ();
  for (int i = 0; i < 280; ++i) HumanCopycat::Record (f.bot->Ent (),&cmd);
  CHECK (HumanCopycat::TestClipCount () == count); // no bot self-pollution
  HumanCopycat::Save ();
  rbcc::Database db; std::string error;
  CHECK (db.load (HumanCopycat::TestDataPath (),"de_hc_recording",HumanCopycat::TestEnvironment (),error));
  CHECK (db.size () == count);
  CHECK (!db.load (HumanCopycat::TestDataPath (),"de_hc_recording",HumanCopycat::TestEnvironment () ^ 1u,error));
  CHECK (db.size () == count); // failed load retains the existing database
  std::filesystem::remove (HumanCopycat::TestDataPath ());
}
}
namespace bot {
TEST_CASE ("human/rbcc_legacy") {
  const auto fixture = std::filesystem::path (__FILE__).parent_path ().parent_path ().parent_path () / "fixtures/legacy-rbcc0003.rbcc";
  rbcc::Database db; std::string error;
  HOST_REQUIRE (db.load (fixture.string (),"de_dust2",0x1234abcdu,error));
  CHECK (db.size () == 1); CHECK (db.frameCount () == 30);
  CHECK (db.clips ().front ().profile == 424); CHECK (db.clips ().front ().frames.front ().forward == 50.0f);
  CHECK (!db.load (fixture.string (),"de_incorrect",0x1234abcdu,error)); CHECK (db.size () == 1);
}
TEST_CASE ("human/practice_information") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_practice")); auto e = f.Foe ();
  for (int i = 4; i < 8; ++i) {
    Path p {}; p.origin = {100.0f * static_cast<float> (i),0.0f,0.0f}; p.number = i;
    for (auto &l : p.links) l.index = kInvalidNodeIndex;
    graph.paths_.push (p);
  }
  graph.PopulateNodes (); practice.Load (); HumanCopycatHook::CurrentNode (*f.bot,0);
  f.Wall (true); practice.UpdateDamage (f.bot,e,100);
  CHECK (practice.GetDamage (Team::Terrorist,0,0) > 0);
  CHECK (practice.GetDamage (Team::Terrorist,0,5) == 0);
  f.Wall (false); practice.UpdateDamage (f.bot,e,100);
  CHECK (practice.GetDamage (Team::Terrorist,0,5) > 0);
  e->v.health = 100.0f; const float score = HumanCopycatHook::Score (*f.bot,e);
  e->v.health = 1.0f; CHECK (HumanCopycatHook::Score (*f.bot,e) == Approx (score));
}
}
namespace bot {
TEST_CASE ("human/split_and_round_variation") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_split",true)); HOST_REQUIRE (game.MapIs (MapFlags::Demolition));
  graph.Reset ();
  const ystl::Vector positions[] = {{0,0,0},{-500,200,0},{-1000,1000,0},{1000,1000,0},{0,500,0},{500,200,0}};
  for (int i=0;i<6;++i) {
    Path p {}; p.number=i; p.origin=positions[i];
    if (i==2||i==3) p.flags=ystl::to_underlying (NodeFlag::Goal);
    for (auto &l:p.links) l.index=kInvalidNodeIndex;
    graph.paths_.push (p);
  }
  graph.PopulateNodes (); planner.Init ();
  for (int i=0;i<4;++i) bots.Addbot (ystl::strings.format ("Split%d",i),Difficulty::Hard,Personality::Normal,Team::Terrorist,Skin::Terror,true);
  HOST_REQUIRE (testhost::PumpBots (f.engine,5)); clients.Update ();
  bots.ForEach ([&] (Bot *b) {
    HumanCopycatHook::Setup (*b); b->pev->flags=FL_CLIENT|FL_FAKECLIENT|FL_ONGROUND;
    b->pev->health=100; b->pev->deadflag=DEAD_NO; b->pev->maxspeed=250; b->pev->origin.clear ();
    clients[b->Ent ()].team=clients[b->Ent ()].team2=Team::Terrorist; return false;
  });
  HumanCopycat::NewRound ();
  int counts[3] {}; int previous[32]; std::fill (std::begin (previous),std::end (previous),-1);
  bots.ForEach ([&] (Bot *b) {
    CHECK (graph.Exists (HumanCopycat::StrategicGoal (*b)));
    const int role=HumanCopycat::TestRole (*b); CHECK (role>=0&&role<3); if(role<0||role>=3) return false; ++counts[role]; previous[b->Index ()]=role; return false;
  });
  CHECK (counts[0]==2); CHECK (counts[1]==2); CHECK (counts[2]==1);
  const float old_weight=HumanCopycat::RouteWeight (Team::Terrorist,2); HumanCopycat::NewRound ();
  unsigned changed=0;
  bots.ForEach ([&] (Bot *b) {HumanCopycat::StrategicGoal (*b); if (previous[b->Index ()]!=HumanCopycat::TestRole (*b)) ++changed; return false;});
  CHECK (changed>0); CHECK (HumanCopycat::RouteWeight (Team::Terrorist,2)!=Approx (old_weight));
}
TEST_CASE ("human/learned_navigation_execution") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc_navexec")); auto teacher=f.Foe ({0,0,0});
  clients[teacher].team=clients[teacher].team2=Team::Terrorist;
  usercmd_t command {}; command.msec=100; command.forwardmove=200;
  teacher->v.velocity={200,0,0};
  for (int i=0;i<=10;++i) {teacher->v.origin={float(i)*64,0,0}; HumanCopycat::Record (teacher,&command);}
  CHECK (HumanCopycat::TestHumanNodes ()>=8);
  HumanCopycatHook::Look (*f.bot,{1000,0,17});
  f.bot->Task ()->data=3; graph.paths_[3].origin={1000,0,0};
  unsigned floor_checks=0,hull_checks=0;
  f.engine.SetTraceHullHook ([&](const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr){
    ++hull_checks; testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
  });
  f.engine.SetTraceLineHook ([&](const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr){
    ++floor_checks;
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    if (b[2]<a[2]-50.0f) tr->flFraction=.36f;
  });
  // PumpBots may already have attempted navigation against persisted data.
  // Start this direct-controller scenario without its normal retry cooldown.
  HumanCopycat::ResetBot (*f.bot);
  // Functional geometry assertions use a relaxed wall clock for cold ASan
  // instrumentation. Production retains 2 ms and the 192-check work bound.
  HumanCopycat::TestRouteBudget (100);
  auto cmd=MakeCommand (); ystl::Vector desired {};
  CHECK (HumanCopycat::Navigate (*f.bot,cmd,desired)); CHECK (HumanCopycat::NavigatingHuman (*f.bot)); CHECK (cmd.forward>0);
  CHECK (hull_checks>0); CHECK (floor_checks>=3);
  f.engine.SetTraceHullHook ([](const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr){
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr); tr->flFraction=.2f;
  });
  CHECK (!HumanCopycat::Navigate (*f.bot,cmd,desired)); CHECK (!HumanCopycat::NavigatingHuman (*f.bot));
}
}

namespace bot {
namespace {
ystl::Array<ystl::String> policy_commands;
void CapturePolicyCommand (edict_t *) {
  ystl::String command (game.BotArgs ());
  if (ystl::StringRef (game.BotArgv (0)).starts_with ("say")) command.assignf ("%s %s", game.BotArgv (0), game.BotArgs ());
  policy_commands.push (command);
}
struct PolicyCapture {
  decltype (dllapi.pfnClientCommand) original {dllapi.pfnClientCommand};
  PolicyCapture () { policy_commands.clear (); dllapi.pfnClientCommand = &CapturePolicyCommand; }
  ~PolicyCapture () { dllapi.pfnClientCommand = original; }
};
}

TEST_CASE ("human426/loadout_touch_and_final_guard") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc426_policy"));
  cv_restricted_weapons.Set ("");
  int firearms = 0;
  for (const auto &weapon : conf.GetWeapons ()) {
    if (human_policy::Firearm (weapon.id)) ++firearms;
    if (weapon.id != Weapon::Knife) CHECK (HumanCopycatHook::Restricted (*f.bot, weapon.id) == !human_policy::Firearm (weapon.id));
    auto box = f.engine.SpawnEntity ("weaponbox"); HOST_REQUIRE (box != nullptr);
    ystl::String model; model.assignf ("models/w_%s", weapon.model);
    box->v.model = f.engine.AllocString (model.chars ());
    CHECK (human_policy::BlockPickup (box) == !human_policy::Usable (weapon.id));
    const int touches = testhost::CsCalls (f.cs, "Touch");
    f.table.pfnTouch (box, f.bot->Ent ());
    CHECK (testhost::CsCalls (f.cs, "Touch") == touches + (human_policy::Usable (weapon.id) ? 1 : 0));
  }
  CHECK (firearms == 6);
  auto box = f.engine.SpawnEntity ("armoury_entity"); HOST_REQUIRE (box != nullptr);
  box->v.model = f.engine.AllocString ("models/w_mp5.mdl");
  CHECK (human_policy::BlockPickup (box));
  auto human = f.Foe ();
  const int touches = testhost::CsCalls (f.cs, "Touch");
  f.table.pfnTouch (box, human);
  CHECK (testhost::CsCalls (f.cs, "Touch") == touches + 1); // humans keep their ordinary loadout
  box->v.model = f.engine.AllocString ("models/w_custom_unknown.mdl");
  CHECK (human_policy::BlockPickup (box));
  box->v.model = f.engine.AllocString ("models/w_backpack.mdl");
  CHECK (!human_policy::BlockPickup (box));
  cv_human_copycat.Set (0);
  f.bot->current_weapon_ = Weapon::MP5;
  auto cmd = MakeCommand (IN_ATTACK | IN_ATTACK2 | IN_RELOAD);
  HumanCopycat::Finalize (*f.bot, cmd);
  CHECK ((cmd.buttons & (IN_ATTACK | IN_ATTACK2 | IN_RELOAD)) == 0);
  PolicyCapture capture;
  HumanCopycatHook::Select (*f.bot, Weapon::MP5);
  CHECK (policy_commands.empty ());
  conf.GetWeaponProp (Weapon::AWP).classname = "weapon_awp";
  HumanCopycatHook::Select (*f.bot, Weapon::AWP);
  CHECK (policy_commands.size () == 1);
  f.bot->current_weapon_ = Weapon::Knife;
  HumanCopycatHook::Enemy (*f.bot, human);
  human->v.origin = {50.0f, 0.0f, 0.0f}; human->v.v_angle = {0.0f, 180.0f, 0.0f};
  cmd = MakeCommand (IN_ATTACK2); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK ((cmd.buttons & IN_ATTACK2) == 0);
  human->v.v_angle = {0.0f, 0.0f, 0.0f};
  cmd = MakeCommand (IN_ATTACK2); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK ((cmd.buttons & IN_ATTACK2) != 0);
}

TEST_CASE ("human426/awp_deagle_and_flash_buy_order") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc426_buy"));
  cv_restricted_weapons.Set ("");
  f.bot->money_amount_ = 16000;
  f.bot->pev->weapons = 1 << 17;
  cv_human_awp_percent.Set (100);
  CHECK (HumanCopycatHook::Primary (*f.bot) == Weapon::AWP);
  cv_human_awp_percent.Set (0);
  CHECK (HumanCopycatHook::Primary (*f.bot) == Weapon::AK47);
  f.bot->team_ = Team::CT;
  CHECK (HumanCopycatHook::Primary (*f.bot) == Weapon::M4A1);
  f.bot->team_ = Team::Terrorist;
  PolicyCapture capture;
  HumanCopycatHook::BuyContextSetup (*f.bot);
  f.bot->pev->weapons = 1 << 17;
  cv_human_deagle_percent.Set (100);
  HumanCopycatHook::Secondary (*f.bot);
  CHECK (!policy_commands.empty ());
  if (game.Is (GameFlags::Legacy)) {
    HOST_REQUIRE (policy_commands.size () == 3);
    CHECK (policy_commands[1] == "menuselect 1");
    CHECK (policy_commands[2] == "menuselect 3");
  }
  else { HOST_REQUIRE (policy_commands.size () == 1); CHECK (policy_commands[0] == "deagle"); }
  policy_commands.clear (); cv_human_deagle_percent.Set (0);
  HumanCopycatHook::Secondary (*f.bot); CHECK (policy_commands.empty ());
  f.bot->money_amount_ = 400;
  HumanCopycatHook::BuyNades (*f.bot);
  HOST_REQUIRE (policy_commands.size () == 4);
  CHECK (policy_commands[0] == "buyequip"); CHECK (policy_commands[1] == "menuselect 3");
  CHECK (policy_commands[2] == "buyequip"); CHECK (policy_commands[3] == "menuselect 3");
  policy_commands.clear (); f.bot->money_amount_ = 199;
  HumanCopycatHook::BuyNades (*f.bot); CHECK (policy_commands.empty ());
}

TEST_CASE ("human426/contextual_grenades") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc426_utility"));
  f.engine.SetCvar ("sv_gravity", 800.0f);
  cv_restricted_weapons.Set ("");
  const auto point = ystl::Vector (750.0f, 0.0f, 0.0f);
  f.bot->pev->weapons = (1 << 25) | (1 << 9) | (1 << 4);
  f.bot->health_value_ = 100.0f;
  CHECK (HumanCopycatHook::BestNade (*f.bot) == Weapon::Flashbang);
  HumanCopycatHook::Nades (*f.bot, point);
  CHECK (f.bot->GetTaskId () == TaskId::ThrowFlashbang);
  f.bot->ClearTasks (); f.bot->pev->weapons &= ~ystl::bit (Weapon::Flashbang);
  f.bot->health_value_ = 25.0f;
  CHECK (HumanCopycatHook::BestNade (*f.bot) == Weapon::Smoke);
  HumanCopycatHook::Nades (*f.bot, point);
  CHECK (f.bot->GetTaskId () == TaskId::ThrowSmoke);
  f.bot->ClearTasks (); f.bot->health_value_ = 100.0f;
  CHECK (HumanCopycatHook::BestNade (*f.bot) == Weapon::Explosive);
  HumanCopycatHook::Nades (*f.bot, point);
  CHECK (f.bot->GetTaskId () == TaskId::ThrowExplosive);
  auto mate = f.Foe (point); clients[mate].team = clients[mate].team2 = Team::Terrorist;
  CHECK (!HumanCopycatHook::NadeSafe (*f.bot, Weapon::Flashbang, point));
  CHECK (!HumanCopycatHook::NadeSafe (*f.bot, Weapon::Explosive, point));
  f.bot->ClearTasks (); f.bot->pev->weapons |= ystl::bit (Weapon::Flashbang);
  HumanCopycatHook::Nades (*f.bot, point);
  CHECK (f.bot->GetTaskId () == TaskId::ThrowSmoke); // unsafe flash falls through, never starves safe utility
}

TEST_CASE ("human426/generative_chat_and_privacy") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc426_chat"));
  cv_chat.Set (1); cv_human_generative_chat.Set (1); cv_human_chat_language.Set ("id");
  HumanChat::Reset ();
  ystl::Array<uint32_t> hashes;
  for (int i = 0; i < 80; ++i) {
    ystl::String message;
    HOST_REQUIRE (HumanChat::Generate (*f.bot, Chat::Hello, message));
    CHECK (!message.empty () && message.size () <= 150);
    CHECK (message.find (';') == ystl::String::InvalidIndex);
    CHECK (message.find ('"') == ystl::String::InvalidIndex);
    CHECK (message.find ('\n') == ystl::String::InvalidIndex);
    bool repeated = false;
    const size_t begin = hashes.size () > 64 ? hashes.size () - 64 : 0;
    for (size_t j = begin; j < hashes.size (); ++j) if (hashes[j] == message.hash ()) repeated = true;
    CHECK (!repeated); hashes.push (message.hash ());
  }
  ystl::String message;
  CHECK (HumanChat::Generate (*f.bot, Chat::NoKeyword, message, "hello\"; quit\n"));
  CHECK (message.find ("quit") == ystl::String::InvalidIndex);
  cv_human_chat_language.Set ("en");
  CHECK (HumanChat::Generate (*f.bot, Chat::Dead, message));
  CHECK (message.find ("round") != ystl::String::InvalidIndex || message.find ("after this") != ystl::String::InvalidIndex);
  cv_human_chat_language.Set ("id");
  auto human = f.Foe (); clients[human].team = clients[human].team2 = Team::Terrorist;
  msgs.Add ("SayText", 76);
  HumanCopycatHook::NoChatCooldown (*f.bot);
  f.engine.SetCmdArgs ({"say_team", "flash dong"});
  bots.CaptureChatRadio ("say_team", "flash dong", human);
  CHECK (f.bot->say_text_buffer_.team_only);
  f.engine.AdvanceTime (12.0f);
  HumanCopycatHook::Chat (*f.bot); // living bots must also process human replies
  CHECK (!HumanCopycatHook::Pending (*f.bot).empty ());
  CHECK (HumanCopycatHook::PendingTeam (*f.bot));
  PolicyCapture capture;
  HumanChat::Poll (*f.bot); CHECK (policy_commands.empty ()); // typing delay
  const auto messages_before = f.engine.Messages ().size ();
  f.engine.AdvanceTime (7.0f); HumanCopycatHook::States (*f.bot, Sense::Invalid);
  HumanChat::Poll (*f.bot);
  CHECK (f.engine.Messages ().size () > messages_before);
  CHECK (policy_commands.empty ()); // generated chat uses SayText directly on every engine
  CHECK (HumanCopycatHook::Pending (*f.bot).empty ());
  CHECK (!HumanChat::Queue (*f.bot, Chat::Hello, false)); // per-bot cooldown
}

TEST_CASE ("human426/counterstrafe_inputs") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc426_movement"));
  cv_human_movement.Set (1);
  HumanCopycatHook::OpenMovement (*f.bot);
  auto foe = f.Foe ({600.0f, 0.0f, 0.0f});
  HumanCopycatHook::Enemy (*f.bot, foe); HumanCopycatHook::Look (*f.bot, foe->v.origin);
  f.bot->pev->velocity = {180.0f, 0.0f, 0.0f};
  const auto velocity = f.bot->pev->velocity;
  auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (cmd.forward < 0.0f); CHECK (!(cmd.buttons & IN_JUMP));
  CHECK (f.bot->pev->velocity == velocity); // real GoldSrc physics, no velocity edits
  HumanCopycatHook::Objective (*f.bot, true);
  cmd = MakeCommand (IN_USE); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (cmd.forward > 0.0f);
  HumanCopycatHook::Objective (*f.bot, false);
  HumanCopycatHook::Enemy (*f.bot, nullptr);
  HumanCopycatHook::States (*f.bot, Sense::HearingEnemy);
  HumanCopycat::ResetBot (*f.bot);
  HumanCopycatHook::Rng (*f.bot).force_chance (true);
  unsigned duck_edges = 0; bool was_ducking = false;
  for (int i = 0; i < 12; ++i) {
    cmd = MakeCommand (); HumanCopycat::Finalize (*f.bot, cmd);
    const bool ducking = (cmd.buttons & IN_DUCK) != 0;
    if (ducking && !was_ducking) ++duck_edges;
    was_ducking = ducking;
    CHECK (!(cmd.buttons & IN_JUMP)); CHECK (f.bot->pev->velocity == velocity);
    f.engine.AdvanceTime (0.025f);
  }
  HumanCopycatHook::Rng (*f.bot).clear_forced ();
  CHECK (duck_edges == 2);
}
TEST_CASE ("human427/locked_roster_and_last_free_name") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc427_names"));
  const auto path = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config, "names.cfg");
  ystl::File::make_path (ystl::String (path.substr (0, path.find_last_of (kPathSeparator))).chars ());
  { ystl::File file (path, "wb"); HOST_REQUIRE (!!file); file.puts ("raw Names {\n  OldAiBoDefault\n}\n"); }
  cv_human_roster_lock.Set (1); conf.LoadNamesConfig ();
  ystl::Array<ystl::String> names;
  ystl::rg.force_int (0); // repeatedly start at the same occupied slot
  for (int i = 0; i < 28; ++i) {
    auto name = conf.PickBotName (); HOST_REQUIRE (name != nullptr);
    bool canonical = false;
    for (const auto &legend : human_policy::LegendNames) if (legend == name->name) canonical = true;
    CHECK (canonical);
    for (const auto &previous : names) CHECK (previous != name->name);
    names.push (name->name); name->used_by = i;
  }
  CHECK (conf.PickBotName () == nullptr); ystl::rg.clear_forced ();
  conf.LoadNamesConfig (); cv_name_prefix.Set ("[POD]");
  bots.Addbot ("SavedAiBoDefault", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (f.engine, 2));
  for (auto &created : bots) {
    if (&created == f.bot) continue;
    bool canonical = false;
    for (const auto &legend : human_policy::LegendNames) if (legend == created.pev->netname.str ()) canonical = true;
    CHECK (canonical);
  }
}

TEST_CASE ("human427/rifle_majority_and_carried_awp_budget") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc427_roles"));
  cv_restricted_weapons.Set (""); cv_human_awp_percent.Set (25);
  for (int i = 0; i < 4; ++i) {
    ystl::String name; name.assignf ("Role%d", i);
    bots.Addbot (name, Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  }
  HOST_REQUIRE (testhost::PumpBots (f.engine, 5));
  for (int pass = 0; pass < 2; ++pass) {
    for (auto &mate : bots) {
      HumanCopycatHook::Setup (mate); mate.team_ = pass ? Team::CT : Team::Terrorist;
      mate.money_amount_ = 16000; mate.pev->weapons = 1 << 17;
    }
    int rifles = 0, snipers = 0;
    for (auto &mate : bots) {
      const auto selected = HumanCopycatHook::Primary (mate);
      CHECK (selected == (pass ? Weapon::M4A1 : Weapon::AK47) || selected == Weapon::AWP);
      if (selected == Weapon::AWP) ++snipers; else ++rifles;
      mate.pev->weapons |= static_cast<int> (ystl::bit (selected)); // the real GameDLL grants it synchronously
    }
    CHECK (rifles >= 3); CHECK (snipers >= 1 && snipers <= 2);
    // A new rich buyer must account for existing surviving snipers.
    f.bot->pev->weapons = 1 << 17;
    for (auto &mate : bots) if (&mate != f.bot) mate.pev->weapons = 1 << 18;
    CHECK (HumanCopycatHook::Primary (*f.bot) == (pass ? Weapon::M4A1 : Weapon::AK47));
  }
  for (unsigned round = 0; round < 20; ++round) {
    CHECK (human_policy::SniperQuota (5, 25, round) >= 1);
    CHECK (human_policy::SniperQuota (5, 25, round) <= 2);
    CHECK (human_policy::SniperQuota (5, 100, round) == 2);
  }
}

TEST_CASE ("human427/immutable_buy_and_forbidden_inventory") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc427_commands"));
  game.ClearGameFlag (GameFlags::Legacy); game.AddGameFlag (GameFlags::Modern);
  HumanCopycatHook::BuyContextSetup (*f.bot); cv_restricted_weapons.Set ("");
  PolicyCapture capture;
  for (auto id : {Weapon::AK47, Weapon::M4A1, Weapon::AWP, Weapon::Deagle, Weapon::USP, Weapon::Glock18}) {
    auto &w = conf.FindWeaponById (id);
    w.buy_group = 5; w.buy_select_t = 1; w.buy_select_ct = 1; w.alias = "m249"; w.name = "weapon_m249";
    policy_commands.clear (); HumanCopycatHook::Buy (*f.bot, &w);
    HOST_REQUIRE (policy_commands.size () == 1); CHECK (policy_commands[0] == human_policy::BuyAlias (id));
  }
  policy_commands.clear ();
  for (const auto &w : conf.GetWeapons ()) {
    if (!human_policy::Firearm (w.id)) HumanCopycatHook::Buy (*f.bot, &w);
  }
  CHECK (policy_commands.empty ());
  f.bot->pev->weapons = static_cast<int> (ystl::bit (Weapon::MP5) | ystl::bit (Weapon::P228) | ystl::bit (Weapon::AWP));
  f.bot->EnforceHumanLoadout ();
  HOST_REQUIRE (policy_commands.size () == 2);
  CHECK (policy_commands[0] == "drop weapon_p228"); CHECK (policy_commands[1] == "drop weapon_mp5navy");
}

TEST_CASE ("human427/chat_without_banks_and_alive_saytext") {
  Fixture f; HOST_REQUIRE (f.Start ("de_hc427_chat"));
  cv_chat.Set (1); cv_human_generative_chat.Set (1);
  cv_language.Set ("en"); conf.LoadChatConfig (); // no bank: must not turn the generator off
  CHECK (cv_chat.As<int> () == 1);
  const auto path = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config, folders.lang, "en_chat.cfg");
  ystl::File::make_path (ystl::String (path.substr (0, path.find_last_of (kPathSeparator))).chars ());
  { ystl::File file (path, "wb"); HOST_REQUIRE (!!file); file.puts ("{{{\n"); }
  conf.LoadChatConfig (); CHECK (cv_chat.As<int> () == 1);
  game.ClearGameFlag (GameFlags::Legacy); game.AddGameFlag (GameFlags::Modern); msgs.Add ("SayText", 76);
  auto human = f.Foe (); clients[human].team = clients[human].team2 = Team::Terrorist;
  HumanCopycatHook::States (*f.bot, Sense::Invalid); HumanChat::Reset ();
  PolicyCapture capture; const auto before = f.engine.Messages ().size ();
  f.engine.AdvanceTime (5.0f); HumanChat::Poll (*f.bot);
  HOST_REQUIRE (!HumanCopycatHook::Pending (*f.bot).empty ()); // living bot initiates a greeting
  f.engine.AdvanceTime (7.0f); HumanChat::Poll (*f.bot);
  CHECK (HumanCopycatHook::Pending (*f.bot).empty ()); CHECK (policy_commands.empty ());
  HOST_REQUIRE (f.engine.Messages ().size () >= before + 6);
  CHECK (f.engine.Messages ()[before + 2] == "string=#Cstrike_Chat_All");
  ystl::String speaker; speaker.assignf ("byte=%d", f.bot->Index () + 1);
  CHECK (f.engine.Messages ()[before + 1] == speaker);
  const auto public_end = f.engine.Messages ().size ();
  clients[human].team = clients[human].team2 = Team::CT;
  f.bot->SendToChatDirect ("private team line", true);
  CHECK (f.engine.Messages ().size () == public_end); // no leak to opposing team
}
namespace {
void EnableLearning (Fixture &f) {
  cv_learning.Set (0); cv_learning_lifelong.Set (0); HumanCopycat::Reload ();
  cv_learning.Set (1); cv_learning_rl.Set (1); cv_learning_imitation.Set (1); cv_learning_selfplay.Set (1);
  cv_learning_multiagent.Set (1); cv_learning_adaptation.Set (0); cv_learning_exploration.Set (0);
  cv_human_movement.Set (1); cv_human_replay.Set (0); HumanCopycatHook::OpenMovement (*f.bot);
  HumanCopycat::NewRound ();
}
edict_t *LearningOpponent (Fixture &f, bool fake = false) {
  auto foe = f.Foe ({600.0f, 0.0f, 0.0f});
  if (fake) foe->v.flags |= FL_FAKECLIENT;
  HumanCopycatHook::Enemy (*f.bot, foe); HumanCopycatHook::Look (*f.bot, foe->v.origin);
  return foe;
}
edict_t *DemonstrateMovement (Fixture &f, edict_t *foe, float side) {
  auto teacher = f.Foe ({0.0f, 0.0f, 0.0f});
  clients[teacher].team = clients[teacher].team2 = Team::Terrorist;
  HumanCopycatHook::Enemy (*f.bot, foe);
  usercmd_t cmd {}; cmd.msec = 25; cmd.buttons = IN_ATTACK; cmd.sidemove = side;
  for (int i = 0; i < 24; ++i) { HumanCopycat::Record (teacher, &cmd); f.engine.AdvanceTime (0.26f); }
  return teacher;
}
}
TEST_CASE ("learning/damage_messages_and_death") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_damage")); EnableLearning (f);
  auto foe = LearningOpponent (f); auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  HOST_REQUIRE (HumanCopycat::TestLearningAction (*f.bot) >= 0);
  // Real Damage dispatch to a HUMAN recipient must credit the attacking bot.
  msgs.Add ("Damage", 91); foe->v.dmg_inflictor = f.bot->Ent ();
  msgs.Start (foe, 91); msgs.Collect (0); msgs.Collect (30); msgs.Collect (0); msgs.Stop ();
  f.engine.AdvanceTime (0.8f); cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningUpdates () == 1);
  msgs.Add ("DeathMsg", 92);
  msgs.Start (nullptr, 92); msgs.Collect (game.IndexOfEntity (foe)); msgs.Collect (game.IndexOfEntity (f.bot->Ent ())); msgs.Stop ();
  CHECK (HumanCopycat::TestLearningUpdates () == 2);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == -1);
  HumanCopycat::LearningDeath (foe, f.bot->Ent ()); CHECK (HumanCopycat::TestLearningUpdates () == 2);
}
TEST_CASE ("learning/selfplay_switch") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_selfplay")); EnableLearning (f);
  auto foe = LearningOpponent (f, true); cv_learning_selfplay.Set (0);
  auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  HumanCopycat::LearningDamage (foe, f.bot->Ent (), 40, 0);
  f.engine.AdvanceTime (0.8f); cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningUpdates () == 0 && HumanCopycat::TestLearningSelfPlay () == 0);
  cv_learning_selfplay.Set (1); HumanCopycat::ResetBot (*f.bot);
  cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  HumanCopycat::LearningDamage (foe, f.bot->Ent (), 40, 0);
  f.engine.AdvanceTime (0.8f); cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningUpdates () == 1 && HumanCopycat::TestLearningSelfPlay () == 1);
}
TEST_CASE ("learning/imitation_changes_emitted_input") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_imitate")); EnableLearning (f);
  auto foe = LearningOpponent (f); auto teacher = DemonstrateMovement (f, foe, 0);
  HOST_REQUIRE (HumanCopycat::TestLearningHumanSamples () >= 20);
  auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == 1); // observed hold action
  CHECK (std::fabs (cmd.forward) < 0.01f && std::fabs (cmd.side) < 0.01f);
  const auto count = HumanCopycat::TestLearningHumanSamples ();
  usercmd_t demo {}; demo.msec = 25; demo.buttons = IN_ATTACK;
  teacher->v.flags |= FL_FAKECLIENT; HumanCopycat::Record (teacher, &demo);
  CHECK (HumanCopycat::TestLearningHumanSamples () == count); // bot commands never become human imitation
}
TEST_CASE ("learning/unsafe_action_mask_and_objective") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_safety")); EnableLearning (f);
  auto foe = LearningOpponent (f); DemonstrateMovement (f, foe, -150);
  // The default synthetic trace has no supporting floor. A demonstrated strafe is forbidden.
  auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == 0);
  CHECK (std::fabs (cmd.side) < 0.01f);
  HumanCopycatHook::Objective (*f.bot, true); cmd = MakeCommand (IN_USE);
  HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == -1);
  CHECK (cmd.forward > 50.0f && (cmd.buttons & IN_USE));
}
TEST_CASE ("learning/visible_observations_only") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_visibility")); EnableLearning (f);
  LearningOpponent (f); cv_learning_adaptation.Set (1);
  auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  const auto observed = HumanCopycat::TestLearningObservations (Team::Terrorist); CHECK (observed == 1);
  f.Wall (true); f.engine.AdvanceTime (0.8f); cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningObservations (Team::Terrorist) == observed);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == -1 && !(cmd.buttons & IN_ATTACK));
  CHECK (HumanCopycat::TestLearningObservations (Team::CT) == 0);
}
TEST_CASE ("learning/strafe_execution_rechecks_floor") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_strafe")); EnableLearning (f);
  auto foe = LearningOpponent (f); DemonstrateMovement (f, foe, -150);
  f.engine.SetTraceLineHook ([] (const float *a, const float *b, int flags, edict_t *skip, TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a, b, flags, skip, tr);
    if (b[2] < a[2] - 90.0f && std::fabs (a[0] - b[0]) < 1.0f && std::fabs (a[1] - b[1]) < 1.0f) tr->flFraction = 0.4f;
  });
  auto cmd = MakeCommand (IN_ATTACK); const auto velocity = f.bot->pev->velocity;
  HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == 2);
  CHECK (cmd.side < -90.0f && f.bot->pev->velocity == velocity);
  // The floor disappears before the 750ms decision interval: stop the unsafe action immediately.
  f.Wall (false); f.engine.AdvanceTime (0.025f); cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == 0);
  CHECK (std::fabs (cmd.side) < 2.0f && cmd.forward > 50.0f);
}
TEST_CASE ("learning/save_reload_and_runtime_disable") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_persist")); EnableLearning (f);
  auto foe = LearningOpponent (f); auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  HumanCopycat::LearningDamage (foe, f.bot->Ent (), 20, 0);
  HumanCopycat::LearningRoundEnd (Team::Terrorist);
  HOST_REQUIRE (HumanCopycat::TestLearningUpdates () == 1);
  cv_learning_lifelong.Set (1); HumanCopycat::Save (); HumanCopycat::Reload ();
  CHECK (HumanCopycat::TestLearningUpdates () == 1);
  auto path = std::filesystem::path (HumanCopycat::TestDataPath ()); path.replace_extension (".hlearn");
  CHECK (std::filesystem::exists (path));
  cv_learning.Set (0); cmd = MakeCommand (); HumanCopycat::Finalize (*f.bot, cmd);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == -1);
  CHECK (cmd.forward > 50.0f); HumanCopycat::LearningDeath (foe, f.bot->Ent ());
  CHECK (HumanCopycat::TestLearningUpdates () == 1);
  std::filesystem::remove (path);
}
TEST_CASE ("learning/slot_identity_and_team_change") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_identity")); EnableLearning (f);
  auto foe = LearningOpponent (f); auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  HOST_REQUIRE (HumanCopycat::TestLearningAction (*f.bot) >= 0);
  ++f.bot->Ent ()->serialnumber;
  HumanCopycat::LearningDamage (foe, f.bot->Ent (), 80, 0);
  CHECK (HumanCopycat::TestLearningAction (*f.bot) == -1); // replacement cannot inherit somebody else's action/reward
  CHECK (HumanCopycat::TestLearningUpdates () == 0);
  HumanCopycat::LearningRoundEnd (Team::Terrorist); HumanCopycat::LearningRoundEnd (Team::Terrorist);
  CHECK (HumanCopycat::TestLearningUpdates () == 0);
}
TEST_CASE ("learning/team_formation_learns_round_outcome") {
  Fixture f; HOST_REQUIRE (f.Start ("de_learn_team", true)); EnableLearning (f);
  graph.Reset ();
  const ystl::Vector positions[] = {{0,0,0},{-500,200,0},{-1000,1000,0},{1000,1000,0},{0,500,0},{500,200,0}};
  for (int i = 0; i < 6; ++i) {
    Path p {}; p.number = i; p.origin = positions[i];
    if (i == 2 || i == 3) p.flags = ystl::to_underlying (NodeFlag::Goal);
    for (auto &link : p.links) link.index = kInvalidNodeIndex;
    graph.paths_.push (p);
  }
  graph.PopulateNodes (); planner.Init ();
  for (int i = 0; i < 4; ++i) bots.Addbot (ystl::strings.format ("LearnTeam%d", i), Difficulty::Hard, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (f.engine, 5)); clients.Update ();
  bots.ForEach ([] (Bot *b) {
    HumanCopycatHook::Setup (*b); b->pev->flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
    b->pev->health = 100; b->pev->deadflag = DEAD_NO; b->pev->maxspeed = 250; b->pev->origin.clear ();
    clients[b->Ent ()].team = clients[b->Ent ()].team2 = Team::Terrorist; return false;
  });
  HumanCopycat::NewRound ();
  bots.ForEach ([] (Bot *b) { HumanCopycat::StrategicGoal (*b); return false; });
  auto foe = LearningOpponent (f); (void)foe;
  auto cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot, cmd);
  HumanCopycat::LearningRoundEnd (Team::CT);
  HOST_REQUIRE (HumanCopycat::TestLearningTeamRounds () == 1);
  HumanCopycat::LearningRoundEnd (Team::CT); CHECK (HumanCopycat::TestLearningTeamRounds () == 1);
  HumanCopycat::NewRound ();
  int counts[3] {};
  bots.ForEach ([&] (Bot *b) {
    CHECK (graph.Exists (HumanCopycat::StrategicGoal (*b)));
    const auto role = HumanCopycat::TestRole (*b); if (role >= 0 && role < 3) ++counts[role]; return false;
  });
  CHECK (counts[0] == 3 && counts[1] == 2 && counts[2] == 0); // learned pressure A replaces losing 2/2/1 split
}
namespace {
void EnableExplore (Fixture &f) {
  cv_explore.Set (0); cv_explore_save.Set (0);
  HumanExplore::Start (game.GetMapName (),HumanCopycat::TestEnvironment (),HumanCopycat::TestDataPath ());
  cv_explore.Set (1); cv_explore_percent.Set (100); cv_explore_probes.Set (32); cv_human_replay.Set (0);
  HumanCopycatHook::OpenMovement (*f.bot); HumanCopycat::NewRound ();
  f.bot->pev->velocity.clear ();
}
void ExploreFloor (Fixture &f, bool gap = false, bool stairs = false) {
  f.engine.SetTraceLineHook ([gap,stairs] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    if (std::fabs (a[0]-b[0]) > 0.01f || std::fabs (a[1]-b[1]) > 0.01f || b[2] >= a[2]) return;
    if (b[0] < -192 || b[0] > 960 || std::fabs (b[1]) > 320 || (gap && b[0] > 24 && b[0] < 48)) return;
    const float floor = -36.0f + (stairs ? std::floor (b[0] / 16.0f) * 12.0f : 0.0f);
    if (a[2] >= floor && b[2] < floor) { tr->flFraction = (a[2]-floor)/(a[2]-b[2]); tr->vecEndPos = {b[0],b[1],floor}; tr->vecPlaneNormal = {0,0,1}; }
  });
}
Bot *DialogueBot (Fixture &f, const char *name, Team team) {
  bots.Addbot (name,Difficulty::Expert,Personality::Normal,team,team == Team::CT ? Skin::Gign : Skin::Terror,true);
  for (int i = 0; i < 80 && !testhost::FindBot (name); ++i) f.engine.AdvanceTime (0.1f), bots.MaintainQuota ();
  auto bot = testhost::FindBot (name); if (!bot) return nullptr;
  HumanCopycatHook::Setup (*bot); bot->team_ = team; bot->pev->flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
  bot->pev->health = 100; bot->pev->deadflag = DEAD_NO;
  clients.Update (); clients[bot->Ent ()].team = clients[bot->Ent ()].team2 = team;
  clients[f.bot->Ent ()].team = clients[f.bot->Ent ()].team2 = Team::Terrorist;
  return bot;
}
void EnableDialogue (Fixture &f) {
  cv_chat.Set (1); cv_human_generative_chat.Set (1); cv_human_chat_catalogue.Set (1); cv_human_chat_conversation.Set (1);
  cv_human_chat_language.Set ("profile"); cv_human_chat_interval.Set (5);
  game.ClearGameFlag (GameFlags::Legacy); game.AddGameFlag (GameFlags::Modern); msgs.Add ("SayText",95);
  f.bot->pev->netname = string_t (f.engine.AllocString ("Hiko"));
  HumanChat::Reset (); HumanCopycatHook::States (*f.bot,Sense::Invalid); HumanCopycatHook::NoChatCooldown (*f.bot);
}
}
TEST_CASE ("explore/offgraph_input_and_coverage") {
  Fixture f; HOST_REQUIRE (f.Start ("de_explore_walk")); EnableExplore (f); ExploreFloor (f);
  const int native = graph.Length (); const auto last = graph[native-1].origin;
  bool outside = false, commanded = false;
  for (int i = 0; i < 700; ++i) {
    HumanCopycat::Frame (); auto cmd = MakeCommand (); HumanCopycat::Finalize (*f.bot,cmd);
    if (HumanExplore::Active (*f.bot)) {
      commanded = commanded || cmd.forward > 50;
      auto a = cmd.angles; a.x = a.z = 0;
      f.bot->pev->origin += (a.forward () * cmd.forward + a.right () * cmd.side) * 0.05f;
      if (f.bot->pev->origin.x > 340 || std::fabs (f.bot->pev->origin.y) > 140) outside = true;
    }
    f.engine.AdvanceTime (0.05f);
  }
  CHECK (commanded && outside); CHECK (HumanExplore::TestNodes () > 40 && HumanExplore::TestEdges () > 50);
  CHECK (graph.Length () == native && graph[native-1].origin == last); // .GRAPH is never a movement fence or modified
  CHECK (f.bot->pev->velocity.empty ());
}
TEST_CASE ("explore/floors_steps_gaps_and_crouch") {
  Fixture f; HOST_REQUIRE (f.Start ("de_explore_probe")); EnableExplore (f); ExploreFloor (f);
  CHECK (HumanExplore::TestProbe (*f.bot,{0,0,-36},{64,0,-36}));
  ExploreFloor (f,true); CHECK (!HumanExplore::TestProbe (*f.bot,{0,0,-36},{64,0,-36}));
  ExploreFloor (f,false,true); CHECK (HumanExplore::TestProbe (*f.bot,{0,0,-36},{64,0,12}));
  ExploreFloor (f);
  f.engine.SetTraceHullHook ([] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    if (b[0] >= 16 && b[2] > -10) tr->flFraction = 0.1f; // standing cannot fit; crouching can
  });
  CHECK (HumanExplore::TestProbe (*f.bot,{0,0,-36},{64,0,-36}));
  f.engine.SetTraceHullHook ([] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr); tr->fAllSolid = true;
  });
  CHECK (!HumanExplore::TestProbe (*f.bot,{0,0,-36},{64,0,-36}));
}
TEST_CASE ("explore/objective_and_combat_priority") {
  Fixture f; HOST_REQUIRE (f.Start ("de_explore_priority")); EnableExplore (f); ExploreFloor (f);
  for (int i = 0; i < 12; ++i) { HumanCopycat::Frame (); f.engine.AdvanceTime (0.06f); }
  auto cmd = MakeCommand (); HumanCopycat::Finalize (*f.bot,cmd); HOST_REQUIRE (HumanExplore::Active (*f.bot));
  HumanCopycatHook::Objective (*f.bot,true); cmd = MakeCommand (IN_USE); HumanCopycat::Finalize (*f.bot,cmd);
  CHECK (!HumanExplore::Active (*f.bot) && (cmd.buttons & IN_USE));
  HumanCopycatHook::Objective (*f.bot,false); f.bot->has_c4_ = true; CHECK (!HumanExplore::Scout (*f.bot));
  f.bot->has_c4_ = false; auto enemy = f.Foe (); HumanCopycatHook::Enemy (*f.bot,enemy);
  cmd = MakeCommand (IN_ATTACK); HumanCopycat::Finalize (*f.bot,cmd); CHECK (!HumanExplore::Active (*f.bot));
  cv_explore.Set (0); CHECK (!HumanExplore::Scout (*f.bot));
}
TEST_CASE ("explore/save_reload_geometry") {
  Fixture f; HOST_REQUIRE (f.Start ("de_explore_save")); EnableExplore (f); ExploreFloor (f);
  for (int i = 0; i < 20; ++i) { HumanCopycat::Frame (); f.engine.AdvanceTime (0.06f); }
  const auto count = HumanExplore::TestNodes (); HOST_REQUIRE (count > 10);
  cv_explore_save.Set (1); HumanCopycat::Save (); HumanCopycat::Reload ();
  CHECK (HumanExplore::TestNodes () == count);
}
TEST_CASE ("dialogue/profile_language_and_roster") {
  Fixture f; HOST_REQUIRE (f.Start ("de_dialogue_profiles")); EnableDialogue (f);
  const char *names[] = {"Hiko","Dosia","f0rest","NEO","shox","markeloff","GuardiaN","zonic","BnTeT","somebody","XigN","Laz","ISSAA","FalleN","XANTARES","gob b"};
  const char *languages[] = {"en","ru","sv","pl","fr","uk","sk","da","id","zh","ko","ja","ar","pt","tr","de"};
  for (size_t i = 0; i < 16; ++i) {
    f.bot->pev->netname = string_t (f.engine.AllocString (names[i])); CHECK (std::strcmp (HumanChat::Language (*f.bot),languages[i]) == 0);
    ystl::String text; CHECK (HumanChat::Generate (*f.bot,Chat::Hello,text)); CHECK (!text.empty () && text.size () <= 150);
  }
  cv_human_roster_lock.Set (1); cv_human_international_roster.Set (1); conf.LoadNamesConfig ();
  ystl::Array<ystl::String> picked;
  for (int i = 0; i < 42; ++i) { auto n = conf.PickBotName (); HOST_REQUIRE (n); for (const auto &old : picked) CHECK (old != n->name); picked.push (n->name); n->used_by = i; }
  CHECK (conf.PickBotName () == nullptr);
}
TEST_CASE ("dialogue/cross_language_reply_is_bounded") {
  Fixture f; HOST_REQUIRE (f.Start ("de_dialogue_reply")); auto reply = DialogueBot (f,"Dosia",Team::CT); HOST_REQUIRE (reply);
  auto observer = f.Foe (); (void)observer; EnableDialogue (f); HumanCopycatHook::States (*reply,Sense::Invalid);
  HOST_REQUIRE (HumanChat::Queue (*f.bot,Chat::Hello,false)); f.engine.AdvanceTime (7); HumanChat::Poll (*f.bot);
  HOST_REQUIRE (HumanChat::TestConversation ()); f.engine.AdvanceTime (2); HumanChat::Poll (*reply);
  HOST_REQUIRE (!HumanCopycatHook::Pending (*reply).empty ()); CHECK (std::strcmp (HumanChat::Language (*reply),"ru") == 0);
  CHECK (HumanCopycatHook::Pending (*reply).find ("привет") != ystl::String::InvalidIndex);
  f.engine.AdvanceTime (5); HumanChat::Poll (*reply);
  CHECK (HumanChat::TestReplies () == 1 && !HumanChat::TestConversation ());
  HumanChat::Poll (*f.bot); HumanChat::Poll (*reply); CHECK (HumanChat::TestReplies () == 1);
}
TEST_CASE ("dialogue/team_scope_and_life_state") {
  Fixture f; HOST_REQUIRE (f.Start ("de_dialogue_privacy")); auto reply = DialogueBot (f,"Dosia",Team::CT); HOST_REQUIRE (reply);
  EnableDialogue (f); HumanCopycatHook::States (*reply,Sense::Invalid);
  HOST_REQUIRE (HumanChat::Queue (*f.bot,Chat::Hello,true)); f.engine.AdvanceTime (7); HumanChat::Poll (*f.bot);
  f.engine.AdvanceTime (2); HumanChat::Poll (*reply); CHECK (HumanCopycatHook::Pending (*reply).empty ());
  CHECK (HumanChat::TestReplies () == 0);
  HumanChat::Reset (); HumanCopycatHook::NoChatCooldown (*f.bot); HumanCopycatHook::NoChatCooldown (*reply);
  HOST_REQUIRE (HumanChat::Queue (*f.bot,Chat::Hello,false)); f.engine.AdvanceTime (7); HumanChat::Poll (*f.bot);
  f.engine.AdvanceTime (2); HumanChat::Poll (*reply); HOST_REQUIRE (!HumanCopycatHook::Pending (*reply).empty ());
  reply->is_alive_ = false; reply->pev->health = 0;
  f.engine.AdvanceTime (5); HumanChat::Poll (*reply);
  CHECK (HumanCopycatHook::Pending (*reply).empty () && HumanChat::TestReplies () == 0);
}

namespace {
void EnableFinal (Fixture &f) {
  cv_final_brain.Set (1); cv_final_variety.Set (1); cv_final_emotions.Set (1);
  cv_final_boost.Set (0); cv_final_lobby.Set (0); cv_final_memory.Set (0); cv_explore.Set (0);
  HumanArena::Start (game.GetMapName (),HumanCopycat::TestEnvironment (),HumanCopycat::TestDataPath ());
  HumanCopycatHook::OpenMovement (*f.bot); HumanCopycatHook::States (*f.bot,Sense::Invalid);
  f.bot->pev->velocity.clear (); HumanArena::NewRound ();
}
void BoostTerrain (Fixture &f, bool ledge, float rise = 80) {
  f.engine.SetTraceLineHook ([ledge,rise] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    if (std::fabs (a[0]-b[0]) > 0.01f || std::fabs (a[1]-b[1]) > 0.01f || b[2] >= a[2]) return;
    const auto radius = std::sqrt (b[0]*b[0]+b[1]*b[1]);
    const float floor = ledge && radius >= 80 && radius <= 140 ? -36.0f+rise : -36.0f;
    if (a[2] >= floor && b[2] < floor) {
      tr->flFraction = (a[2]-floor)/(a[2]-b[2]); tr->vecEndPos = {b[0],b[1],floor}; tr->vecPlaneNormal = {0,0,1};
    }
  });
}
}
TEST_CASE ("final/camp_novelty_survives_reset_and_expires") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_camp")); EnableFinal (f);
  HumanCopycatHook::CampTask (*f.bot); HOST_REQUIRE (HumanArena::Camp (*f.bot));
  CHECK (!HumanArena::Fresh (*f.bot,{0,0,-36})); CHECK (HumanArena::Camp (*f.bot));
  HumanArena::ResetBot (*f.bot); HumanCopycatHook::CampTask (*f.bot);
  HumanCopycatHook::RunCamp (*f.bot); CHECK (HumanCopycatHook::TaskIdOf (*f.bot) != TaskId::Camp);
  f.bot->pev->origin = {400,0,0}; HumanCopycatHook::CampTask (*f.bot);
  HOST_REQUIRE (HumanArena::Camp (*f.bot)); f.engine.AdvanceTime (10);
  HumanCopycatHook::RunCamp (*f.bot); CHECK (HumanCopycatHook::TaskIdOf (*f.bot) != TaskId::Camp);
  HumanArena::ResetBot (*f.bot); f.bot->pev->origin.clear ();
  CHECK (!HumanArena::Camp (*f.bot));
  f.bot->has_c4_ = true; CHECK (HumanArena::Camp (*f.bot)); // objective defense can reuse necessary cover
}
TEST_CASE ("final/native_goal_variety_and_objective_priority") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_goals",true)); EnableFinal (f);
  graph.Reset ();
  for (int i = 0; i < 16; ++i) {
    Path p {}; p.origin = {300.0f*float (i%4),300.0f*float (i/4),0}; p.number = i;
    if (i == 3 || i == 15) p.flags = ystl::to_underlying (NodeFlag::Goal);
    for (auto &l : p.links) l.index = kInvalidNodeIndex;
    graph.paths_.push (p);
  }
  graph.PopulateNodes (); planner.Init (); std::set<int> visited;
  f.engine.SetCvar ("mp_roundtime",3.0f); f.engine.SetCvar ("mp_freezetime",0.0f); game_state.RoundStart ();
  HumanCopycatHook::Setup (*f.bot); HumanCopycatHook::OpenMovement (*f.bot); f.bot->has_c4_ = false;
  for (int round = 0; round < 12; ++round) {
    HumanCopycat::NewRound (); const auto goal = HumanArena::Goal (*f.bot);
    if (goal < 0) continue; // support strategy deliberately keeps native team behavior
    HumanCopycat::BeforeThink (*f.bot); CHECK (HumanCopycatHook::GoalData (*f.bot) == goal);
    CHECK (visited.count (goal) == 0); visited.insert (goal);
    HumanArena::Visit (*f.bot,graph[goal].origin-ystl::Vector (0,0,36),0);
    HumanArena::ResetBot (*f.bot);
  }
  CHECK (visited.size () >= 8); f.bot->has_c4_ = true;
  CHECK (HumanArena::Goal (*f.bot) == kInvalidNodeIndex);
}
TEST_CASE ("final/scoreboard_mood_and_stale_brag") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_rank")); auto rival = f.Foe (); EnableDialogue (f); EnableFinal (f);
  f.bot->pev->frags = 15; rival->v.frags = 12; HumanArena::BeforeThink (*f.bot);
  CHECK (HumanArena::Top (*f.bot) && HumanArena::Rank (*f.bot) == 1);
  HOST_REQUIRE (!HumanCopycatHook::Pending (*f.bot).empty ());
  rival->v.frags = 20; CHECK (!HumanArena::Top (*f.bot)); HumanChat::Poll (*f.bot);
  CHECK (HumanCopycatHook::Pending (*f.bot).empty ()); // overtaken while typing: boast is canceled
  const float risk = HumanArena::Risk (*f.bot);
  HumanArena::Damage (f.bot->Ent (),rival,80); f.bot->pev->health = 20;
  f.engine.AdvanceTime (0.8f); HumanArena::BeforeThink (*f.bot);
  CHECK (HumanArena::Mood (*f.bot) == 3 && HumanArena::Risk (*f.bot) < risk);
  CHECK (HumanArena::Rank (*f.bot) == 2 && f.bot->pev->frags == Approx (15.0f));
  HumanArena::Death (f.bot->Ent (),rival); CHECK (f.bot->pev->frags == Approx (15.0f));
}
TEST_CASE ("final/sniper_identifies_static_door_gap") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_gap")); EnableFinal (f);
  f.bot->current_weapon_ = Weapon::AWP; HumanCopycatHook::CampTask (*f.bot);
  f.engine.SetTraceLineHook ([] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    const bool gap = b[0]-a[0] > 1300 && std::fabs (b[1]-a[1]) < 1 && std::fabs (a[1]) < 10;
    if (!gap) tr->flFraction = 0.2f;
  });
  HOST_REQUIRE (HumanArena::Camp (*f.bot)); ystl::Vector desired {10,90,0}; HumanArena::IdleLook (*f.bot,desired);
  CHECK (std::fabs (desired.y) < 1.0f && std::fabs (desired.x) < 1.0f);
  const auto origin = f.bot->pev->origin; auto cmd = MakeCommand (); HumanCopycat::Finalize (*f.bot,cmd);
  CHECK (f.bot->pev->origin == origin && f.bot->pev->velocity.empty ());
}
TEST_CASE ("final/automatic_boost_geometry_contact_and_landing") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_boost")); auto helper = DialogueBot (f,"Dosia",Team::Terrorist); HOST_REQUIRE (helper);
  EnableFinal (f); cv_final_boost.Set (1); HumanCopycatHook::OpenMovement (*helper);
  HumanCopycatHook::States (*helper,Sense::Invalid); helper->pev->origin.clear (); helper->pev->velocity.clear (); helper->pev->maxspeed = 250;
  BoostTerrain (f,true); HumanArena::BeforeThink (*f.bot);
  HOST_REQUIRE (HumanArena::Boosting (*f.bot) && HumanArena::Boosting (*helper));
  ystl::Vector base,approach,landing; HOST_REQUIRE (HumanArena::TestBoostPoints (*f.bot,base,approach,landing));
  CHECK (landing.z-base.z == Approx (80.0f));
  // The fake engine does not simulate CS player collision. Supply explicit physical
  // observations at each step; assert the bot only emits ordinary input commands.
  helper->pev->origin = base+ystl::Vector (0,0,36); f.bot->pev->origin = approach+ystl::Vector (0,0,36);
  auto h = MakeCommand (), c = MakeCommand (); HumanCopycat::Finalize (*helper,h); HumanCopycat::Finalize (*f.bot,c);
  CHECK ((h.buttons & IN_DUCK) && (c.buttons & IN_JUMP));
  helper->pev->flags |= FL_DUCKING; helper->pev->origin = base+ystl::Vector (0,0,18);
  f.bot->pev->origin = base+ystl::Vector (0,0,72); f.bot->pev->groundentity = helper->Ent ();
  f.engine.AdvanceTime (0.1f); c = MakeCommand (); HumanCopycat::Finalize (*f.bot,c);
  f.bot->pev->flags &= ~FL_ONGROUND; f.bot->pev->groundentity = nullptr;
  f.bot->pev->origin = base+ystl::Vector (0,0,114); f.engine.AdvanceTime (0.2f);
  const auto before = f.bot->pev->origin; c = MakeCommand (); h = MakeCommand ();
  HumanCopycat::Finalize (*helper,h); HumanCopycat::Finalize (*f.bot,c);
  CHECK (!(h.buttons & IN_DUCK) && (c.buttons & IN_DUCK));
  CHECK (f.bot->pev->origin == before && f.bot->pev->velocity.empty ());
  f.bot->pev->flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND;
  f.bot->pev->groundentity = f.engine.EdictList (); f.bot->pev->origin = landing+ystl::Vector (0,0,36);
  f.engine.AdvanceTime (0.1f); c = MakeCommand (); HumanCopycat::Finalize (*f.bot,c);
  CHECK (HumanArena::TestBoosts () == 1 && !HumanArena::Boosting (*f.bot) && !HumanArena::Boosting (*helper));
}
TEST_CASE ("final/boost_requires_player_collision_and_aborts_on_combat") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_nocollision")); auto helper = DialogueBot (f,"Dosia",Team::Terrorist); HOST_REQUIRE (helper);
  EnableFinal (f); cv_final_boost.Set (1); HumanCopycatHook::OpenMovement (*helper); HumanCopycatHook::States (*helper,Sense::Invalid);
  helper->pev->maxspeed = 250; helper->pev->origin.clear (); BoostTerrain (f,false);
  HOST_REQUIRE (HumanArena::TestStartBoost (*helper,*f.bot,{0,0,-36},{48,0,44}));
  f.bot->pev->origin = {-40,0,0}; auto c = MakeCommand (); HumanCopycat::Finalize (*f.bot,c);
  HOST_REQUIRE (HumanArena::Boosting (*f.bot)); const auto before = f.bot->pev->origin;
  f.engine.AdvanceTime (4.7f); c = MakeCommand (); HumanCopycat::Finalize (*f.bot,c);
  CHECK (!HumanArena::Boosting (*f.bot) && HumanArena::TestBoosts () == 0 && f.bot->pev->origin == before);
  HOST_REQUIRE (HumanArena::TestStartBoost (*helper,*f.bot,{0,0,-36},{48,0,44}));
  HumanCopycatHook::States (*helper,Sense::SeeingEnemy); c = MakeCommand (); HumanCopycat::Finalize (*f.bot,c);
  CHECK (!HumanArena::Boosting (*f.bot) && !HumanArena::Boosting (*helper));
}
TEST_CASE ("final/lobby_paces_arrivals_and_departures") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_lobby")); f.Foe (); EnableFinal (f);
  cv_final_lobby.Set (1); cv_final_join_min.Set (1); cv_final_join_max.Set (1);
  HumanArena::Joined (*f.bot); CHECK (!HumanArena::ReadyToJoin ());
  CHECK (HumanArena::LobbyTarget (31,1) == 1); f.engine.AdvanceTime (1.1f);
  CHECK (HumanArena::ReadyToJoin () && HumanArena::LobbyTarget (31,1) == 15);
  HOST_REQUIRE (HumanArena::MayLeave (*f.bot)); HumanArena::Leaving (*f.bot);
  CHECK (HumanArena::TestLeaves () == 1 && !HumanArena::ReadyToJoin () && !HumanArena::MayLeave (*f.bot));
  f.engine.AdvanceTime (2.9f); CHECK (!HumanArena::ReadyToJoin ());
  f.engine.AdvanceTime (10); CHECK (HumanArena::ReadyToJoin ());
}
TEST_CASE ("final/one_human_and_31_bots_on_32_slots") {
  Fixture f; HOST_REQUIRE (f.Start ("de_final_32",false,32)); f.Foe (); EnableFinal (f);
  cv_final_lobby.Set (1); cv_autovacate_keep_slots.Set (1); cv_quota.Set (31);
  cv_final_join_min.Set (0.5f); cv_final_join_max.Set (0.5f); cv_join_after_player.Set (0);
  f.engine.SetCvar ("mp_limitteams",0.0f); f.engine.SetCvar ("mp_autoteambalance",0.0f);
  HOST_REQUIRE (HumanArena::Capacity () == 31);
  float joined = -100; int previous = 1;
  for (int i = 0; i < 1000 && bots.GetBotCount () < 31; ++i) {
    f.engine.AdvanceTime (0.1f); bots.MaintainQuota ();
    if (bots.GetBotCount () > previous) {
      CHECK (game.Time () - joined >= 0.49f); joined = game.Time (); previous = bots.GetBotCount ();
    }
  }
  CHECK (bots.GetBotCount () == 31 && bots.GetHumansCount () == 1 && game.MaxClients () == 32);
  bots.Addbot ("OverCapacity",Difficulty::Expert,Personality::Normal,Team::Terrorist,Skin::Terror,true);
  for (int i = 0; i < 10; ++i) { f.engine.AdvanceTime (0.2f); bots.MaintainQuota (); }
  CHECK (!testhost::FindBot ("OverCapacity") && bots.GetBotCount () == 31 && cv_quota.As<int> () == 31);
}
} // namespace bot

namespace bot {
namespace {
void EnableAim431 (Fixture &f,float level=100) {
  cv_human_aim_adaptive.Set (1);cv_human_aim_level.Set (level);cv_human_aim_save.Set (0);cv_human_aim_techniques.Set (1);
  cv_human_aim_wallbang.Set (1);cv_human_aim_quickswitch.Set (1);cv_human_aim_preaim.Set (1);
  cv_human_replay.Set (0);cv_human_record.Set (0);cv_learning.Set (0);cv_explore.Set (0);
  HumanCopycatHook::OpenMovement (*f.bot);HumanCopycatHook::States (*f.bot,Sense::Invalid);
  HumanAimCoach::Start (game.GetMapName (),HumanCopycat::TestEnvironment (),HumanCopycat::TestDataPath ());
  msgs.Add ("CurWeapon",94);msgs.Add ("Damage",95);
}
void AimAmmo (edict_t *e,unsigned w,int clip) {
  msgs.Start (e,94);msgs.Collect (1);msgs.Collect (static_cast<int> (w));msgs.Collect (clip);msgs.Stop ();
}
void AimHit (edict_t *victim,edict_t *attacker,int bits=2) {
  victim->v.dmg_inflictor=attacker;msgs.Start (victim,95);msgs.Collect (0);msgs.Collect (20);msgs.Collect (bits);msgs.Stop ();
}
void AimWall (Fixture &f,float thick=20) {
  f.engine.SetTraceLineHook ([thick] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    float x=0;bool hit=false;
    if (a[0]<200&&b[0]>200) {x=200;hit=true;}
    if (a[0]>200+thick&&b[0]<200+thick) {x=200+thick;hit=true;}
    if (hit) {tr->flFraction=(x-a[0])/(b[0]-a[0]);tr->vecEndPos={x,a[1]+(b[1]-a[1])*tr->flFraction,a[2]+(b[2]-a[2])*tr->flFraction};tr->vecPlaneNormal={-1,0,0};}
  });
  f.engine.SetPointContentsHook ([thick] (const float *p) {return p[0]>=200&&p[0]<=200+thick ? CONTENTS_SOLID : CONTENTS_EMPTY;});
  f.engine.SetTraceTextureHook ([] (edict_t *,const float *,const float *) -> const char * {return "aimwood";});
}
void AimMaterials () {std::filesystem::create_directories ("sound");std::ofstream f ("sound/materials.txt");f<<"W aimwood\nM aimmetal\n";}
void TrainAimHuman (Fixture &f,edict_t *human,int shots) {
  human->v.v_angle={0,180,0};usercmd_t cmd {};cmd.msec=25;cmd.viewangles={0,180,0};cmd.buttons=IN_ATTACK;int ammo=30;AimAmmo (human,28,ammo);
  for(int i=0;i<shots;++i) {
    if(i%10==0) {f.Wall (true);HumanCopycat::Record (human,&cmd);f.Wall (false);f.engine.AdvanceTime (0.05f);}
    HumanCopycat::Record (human,&cmd);f.engine.AdvanceTime (0.2f);HumanCopycat::Record (human,&cmd);
    if (ammo==0) {ammo=30;AimAmmo (human,28,ammo);}
    AimAmmo (human,28,--ammo);if(i%3!=0) AimHit (f.bot->Ent (),human);
  }
}
}
TEST_CASE ("aim431/actual_human_shots_not_attack_buttons") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_shots"));auto human=f.Foe ();EnableAim431 (f,-1);human->v.v_angle={0,180,0};
  usercmd_t cmd {};cmd.msec=25;cmd.buttons=IN_ATTACK;cmd.viewangles={0,180,0};AimAmmo (human,28,30);
  for(int i=0;i<30;++i) {HumanCopycat::Record (human,&cmd);AimAmmo (human,28,30);f.engine.AdvanceTime (0.025f);}
  CHECK (HumanAimCoach::TestShots (human)==0);HumanCopycat::Record (human,&cmd);AimAmmo (human,28,29);
  CHECK (HumanAimCoach::TestShots (human)==1);AimHit (f.bot->Ent (),human);AimHit (f.bot->Ent (),human);
  CHECK (HumanAimCoach::TestHits (human)==1);AimAmmo (human,28,30);CHECK (HumanAimCoach::TestShots (human)==1);
  AimAmmo (f.bot->Ent (),28,30);AimAmmo (f.bot->Ent (),28,29);CHECK (HumanAimCoach::TestShots (human)==1);
  HumanAimCoach::Update (*f.bot,human);CHECK (HumanAimCoach::Level (*f.bot)==Approx (0.0f));
  CHECK (HumanCopycat::TestClipCount ()==0); // calibration is independent of demonstration recording
  f.engine.AdvanceTime (0.15f);HumanCopycat::Record (human,&cmd);AimAmmo (human,28,29);
  CHECK (HumanAimCoach::TestShots (human)==2&&HumanAimCoach::TestHits (human)==1); // duplicate damage cannot credit a later miss
  f.engine.AdvanceTime (0.15f);HumanCopycat::Record (human,&cmd);AimHit (f.bot->Ent (),human);
  msgs.Add ("DeathMsg",96);msgs.Start (nullptr,96);msgs.Collect (game.IndexOfEntity (human));msgs.Collect (game.IndexOfEntity (f.bot->Ent ()));msgs.Collect (1);msgs.Stop ();
  AimAmmo (human,28,28);CHECK (HumanAimCoach::TestShots (human)==3&&HumanAimCoach::TestHeadKills (human)==1);
}
TEST_CASE ("aim431/adapts_from_real_human_evidence") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_adapt"));auto human=f.Foe ();EnableAim431 (f,-1);
  TrainAimHuman (f,human,80);CHECK (HumanAimCoach::TestShots (human)==80&&HumanAimCoach::TestHits (human)>=50);
  CHECK (HumanAimCoach::TestEstimate (human)>0.7f);
  const auto initial=HumanAimCoach::Update (*f.bot,human);CHECK (initial<=0.011f);
  for(int i=0;i<100;++i) {f.engine.AdvanceTime (0.5f);HumanAimCoach::Update (*f.bot,human);}
  CHECK (HumanAimCoach::Level (*f.bot)>0.65f&&HumanAimCoach::Level (*f.bot)<=1);
  const float matched=HumanAimCoach::Level (*f.bot);clients[human].team=clients[human].team2=Team::Terrorist;
  for(int i=0;i<10;++i) {f.engine.AdvanceTime (0.5f);HumanAimCoach::Update (*f.bot,human);}
  CHECK (HumanAimCoach::Level (*f.bot)>matched-0.005f); // teammates also use the server's human reference when needed
  HumanCopycat::ResetBot (*f.bot);CHECK (HumanAimCoach::Level (*f.bot)>0.65f);
}
TEST_CASE ("aim431/human_calibration_respects_crosshair_target") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_reticle"));auto human=f.Foe ();auto nearer=DialogueBot (f,"Nearer",Team::Terrorist);HOST_REQUIRE (nearer);
  EnableAim431 (f,-1);nearer->pev->origin={250,100,0};human->v.v_angle={0,180,0};AimAmmo (human,28,30);
  const auto actual=f.bot->Ent ();
  f.engine.SetTraceLineHook ([actual] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    if (std::fabs (b[0]-a[0])+std::fabs (b[1]-a[1])>4000) {tr->pHit=actual;tr->flFraction=0.06f;}
  });
  usercmd_t cmd {};cmd.msec=25;cmd.buttons=IN_ATTACK;cmd.viewangles={0,180,0};HumanCopycat::Record (human,&cmd);
  f.engine.AdvanceTime (0.2f);HumanCopycat::Record (human,&cmd);AimAmmo (human,28,29);AimHit (actual,human);
  CHECK (HumanAimCoach::TestShots (human)==1&&HumanAimCoach::TestHits (human)==1);
}
TEST_CASE ("aim431/counter_strafe_waits_for_engine_stop") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_stop"));auto enemy=f.Foe ({700,0,0});EnableAim431 (f);
  f.bot->pev->netname=string_t (f.engine.AllocString ("f0rest"));auto learned=MovementClip (*f.bot);
  for(auto &frame:learned.frames) {frame.buttons=rbcc::Attack;frame.previousButtons=rbcc::Attack;frame.context.visible=1;frame.context.targetToken=77;frame.context.enemyDistance=700;frame.context.enemyPitch=2;frame.context.enemy={700,0,10};}
  learned.frames.front ().buttons = learned.frames.front ().previousButtons = 0;
  learned.frames.back ().buttons = learned.frames.back ().previousButtons = 0;
  for (size_t i = 1; i < learned.frames.size (); ++i) learned.frames[i].previousButtons = learned.frames[i-1].buttons;
  learned.features = rbcc::analyze (learned.frames);
  HOST_REQUIRE (HumanCopycat::TestAddClip (learned)>0);
  ystl::Vector point {};CHECK (HumanCopycat::VisualPoint (f.bot->Ent (),enemy,point,true));CHECK (point.z==Approx (25.0f));
  f.engine.SetTraceLineHook ([] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);if (b[2]>20) tr->flFraction=0.5f;
  });
  CHECK (HumanCopycat::VisualPoint (f.bot->Ent (),enemy,point,true)&&point.z==Approx (10.0f));f.Wall (false);
  HumanCopycatHook::Enemy (*f.bot,enemy);HumanCopycatHook::Look (*f.bot,enemy->v.origin);f.bot->pev->velocity={150,0,0};
  const auto origin=f.bot->pev->origin,velocity=f.bot->pev->velocity;auto cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);
  CHECK (!(cmd.buttons&IN_ATTACK));auto yaw=cmd.angles;yaw.x=yaw.z=0;const auto wish=yaw.forward ()*cmd.forward+yaw.right ()*cmd.side;
  CHECK (wish.x<0&&f.bot->pev->velocity==velocity&&f.bot->pev->origin==origin);
  f.bot->pev->velocity.clear ();bool fired=false;
  for(int i=0;i<50;++i) {f.engine.AdvanceTime (0.025f);cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);fired=fired||(cmd.buttons&IN_ATTACK);CHECK (HumanAimCoach::TestContact (*f.bot).z<30);}
  CHECK (fired);f.bot->pev->flags&=~FL_ONGROUND;cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);CHECK (!(cmd.buttons&IN_ATTACK));
}
TEST_CASE ("aim431/awp_confirmed_zoom_switch_and_cooldown") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_awp"));auto enemy=f.Foe ({3000,0,0});EnableAim431 (f);
  f.bot->pev->weapons=(1<<18)|(1<<26)|(1<<29);AimAmmo (f.bot->Ent (),18,10);HumanCopycatHook::Enemy (*f.bot,enemy);
  f.bot->pev->fov=90;auto cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);
  CHECK ((cmd.buttons&IN_ATTACK2)&&!(cmd.buttons&IN_ATTACK));
  for(int i=0;i<10;++i) {f.engine.AdvanceTime (0.025f);cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);CHECK (!(cmd.buttons&IN_ATTACK));}
  f.bot->pev->fov=40;bool fired=false;
  for(int i=0;i<40&&!fired;++i) {f.engine.AdvanceTime (0.025f);cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);fired=(cmd.buttons&IN_ATTACK)!=0;}
  HOST_REQUIRE (fired);AimAmmo (f.bot->Ent (),18,9);PolicyCapture capture;
  f.engine.AdvanceTime (0.14f);cmd=MakeCommand ();HumanCopycat::Finalize (*f.bot,cmd);bool switched=false;
  for(const auto &text:policy_commands) switched=switched||text.find ("weapon_deagle")!=ystl::String::InvalidIndex;
  CHECK (switched&&!(cmd.buttons&IN_ATTACK));AimAmmo (f.bot->Ent (),26,7);
  f.engine.AdvanceTime (0.2f);cmd=MakeCommand ();HumanCopycat::Finalize (*f.bot,cmd);bool back=false;
  for(const auto &text:policy_commands) back=back||text.find ("weapon_awp")!=ystl::String::InvalidIndex;
  CHECK (back);AimAmmo (f.bot->Ent (),18,9);f.bot->pev->fov=40;
  for(int i=0;i<30;++i) {f.engine.AdvanceTime (0.025f);cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);CHECK (!(cmd.buttons&IN_ATTACK));}
}
TEST_CASE ("aim431/wallbang_uses_frozen_sight_and_checks_friendlies") {
  AimMaterials ();Fixture f;HOST_REQUIRE (f.Start ("de_aim431_wall"));auto enemy=f.Foe ();EnableAim431 (f);
  HumanCopycatHook::Enemy (*f.bot,enemy);
  for(int i=0;i<20;++i) {auto cmd=MakeCommand ();HumanCopycat::Finalize (*f.bot,cmd);f.engine.AdvanceTime (0.025f);}
  const auto known=HumanAimCoach::TestContact (*f.bot);HOST_REQUIRE (known.x>450);AimWall (f);
  enemy->v.origin={650,800,0};ystl::Vector point {};uint32_t token=0;
  HOST_REQUIRE (HumanAimCoach::Wallbang (*f.bot,point,token));CHECK (point.distance (known)<0.01f&&std::fabs (point.y)<1);
  bool fired=false;for(int i=0;i<20;++i) {auto cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);fired=fired||(cmd.buttons&IN_ATTACK);f.engine.AdvanceTime (0.025f);}
  CHECK (fired);auto ally=DialogueBot (f,"Safety",Team::Terrorist);HOST_REQUIRE (ally);ally->pev->origin={350,0,0};
  // Refresh only the stored perception time; do not read the hidden enemy origin.
  HumanAimCoach::Seen (*f.bot,token,known);CHECK (!HumanAimCoach::Wallbang (*f.bot,point,token));
  ally->pev->origin={350,300,0};AimWall (f,100);CHECK (!HumanAimCoach::Wallbang (*f.bot,point,token));
}
TEST_CASE ("aim431/sound_memory_expires_without_hidden_tracking") {
  AimMaterials ();Fixture f;HOST_REQUIRE (f.Start ("de_aim431_sound"));auto enemy=f.Foe ({600,800,0});EnableAim431 (f);HumanAimCoach::Update (*f.bot,nullptr);AimWall (f);
  auto &noise=clients[enemy].noise;noise.pos={500,0,0};noise.start=game.Time ();noise.last=game.Time ()+10;noise.dist=2000;noise.type=Noise::WeaponFire;
  HumanCopycatHook::Hear (*f.bot);const auto heard=HumanAimCoach::TestContact (*f.bot);CHECK (heard.x>450&&std::fabs (heard.y)<100);
  ystl::Vector point {};uint32_t token=0;CHECK (HumanAimCoach::Wallbang (*f.bot,point,token));
  enemy->v.origin={900,-800,0};f.engine.AdvanceTime (1.2f);HumanCopycatHook::Hear (*f.bot);
  CHECK (HumanAimCoach::TestContact (*f.bot)==heard&&!HumanAimCoach::Wallbang (*f.bot,point,token));
}
TEST_CASE ("aim431/persistence_and_minimum_level") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_save"));auto human=f.Foe ();EnableAim431 (f,0);TrainAimHuman (f,human,30);
  CHECK (HumanAimCoach::Level (*f.bot)==Approx (0.0f));cv_human_aim_save.Set (1);HumanCopycat::Save ();
  HOST_REQUIRE (!HumanAimCoach::Dirty ());HumanCopycat::Reload ();CHECK (HumanAimCoach::TestShots (human)==30);
  HumanCopycatHook::Enemy (*f.bot,human);auto cmd=MakeCommand ();HumanCopycat::Finalize (*f.bot,cmd);CHECK (HumanAimCoach::Practice (*f.bot)>0);
  const float motor=HumanAimCoach::Practice (*f.bot);HumanCopycat::Save ();HumanCopycat::Reload ();HumanAimCoach::Update (*f.bot,human);
  CHECK (HumanAimCoach::Practice (*f.bot)>=motor*0.99f);
  cv_human_aim_level.Set (100);HumanCopycat::Blind (f.bot->Ent (),2);cmd=MakeCommand (IN_ATTACK);HumanCopycat::Finalize (*f.bot,cmd);CHECK (!(cmd.buttons&IN_ATTACK));
}
TEST_CASE ("aim431/crouch_hop_keeps_real_physics_and_objective_priority") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_hop"));EnableAim431 (f);HumanAimCoach::Update (*f.bot,nullptr);ExploreFloor (f);
  HumanCopycatHook::States (*f.bot,Sense::HearingEnemy);f.bot->pev->velocity={180,0,0};cv_human_aim_crouchhop.Set (1);
  auto cmd=MakeCommand ();ystl::Vector desired {};const auto origin=f.bot->pev->origin,velocity=f.bot->pev->velocity;
  HumanAimCoach::Prepare (*f.bot,cmd,desired,false,false,true,false);CHECK (cmd.buttons&IN_JUMP);
  CHECK (f.bot->pev->origin==origin&&f.bot->pev->velocity==velocity);
  f.bot->pev->flags&=~FL_ONGROUND;f.engine.AdvanceTime (0.1f);cmd=MakeCommand ();HumanAimCoach::Prepare (*f.bot,cmd,desired,false,false,true,false);CHECK (cmd.buttons&IN_DUCK);
  HumanCopycatHook::Objective (*f.bot,true);cmd=MakeCommand (IN_USE);HumanCopycat::Finalize (*f.bot,cmd);CHECK ((cmd.buttons&IN_USE)&&!(cmd.buttons&IN_JUMP));
}
TEST_CASE ("aim431/high_box_boost_requires_second_physical_support") {
  Fixture f;HOST_REQUIRE (f.Start ("de_aim431_highboost"));auto helper=DialogueBot (f,"Dosia",Team::Terrorist);HOST_REQUIRE (helper);
  EnableFinal (f);EnableAim431 (f);cv_final_boost.Set (1);HumanCopycatHook::OpenMovement (*helper);
  HumanCopycatHook::States (*helper,Sense::Invalid);helper->pev->origin.clear ();helper->pev->velocity.clear ();helper->pev->maxspeed=250;
  BoostTerrain (f,true,128);HumanArena::BeforeThink (*f.bot);HOST_REQUIRE (HumanArena::Boosting (*f.bot)&&HumanArena::Boosting (*helper));
  ystl::Vector base,approach,landing;HOST_REQUIRE (HumanArena::TestBoostPoints (*f.bot,base,approach,landing));CHECK (landing.z-base.z==Approx (128.0f));
  helper->pev->origin=base+ystl::Vector (0,0,36);f.bot->pev->origin=approach+ystl::Vector (0,0,36);
  auto h=MakeCommand (),c=MakeCommand ();HumanCopycat::Finalize (*helper,h);HumanCopycat::Finalize (*f.bot,c);CHECK ((h.buttons&IN_DUCK)&&(c.buttons&IN_JUMP));
  helper->pev->flags|=FL_DUCKING;helper->pev->origin=base+ystl::Vector (0,0,18);
  f.bot->pev->origin=base+ystl::Vector (0,0,72);f.bot->pev->groundentity=helper->Ent ();f.engine.AdvanceTime (0.1f);
  c=MakeCommand ();HumanCopycat::Finalize (*f.bot,c);
  f.bot->pev->flags&=~FL_ONGROUND;f.bot->pev->groundentity=nullptr;f.bot->pev->origin=base+ystl::Vector (0,0,114);f.engine.AdvanceTime (0.2f);
  c=MakeCommand ();h=MakeCommand ();HumanCopycat::Finalize (*helper,h);HumanCopycat::Finalize (*f.bot,c);
  CHECK (!(h.buttons&IN_DUCK)&&!(c.buttons&IN_DUCK)&&std::fabs (c.forward)+std::fabs (c.side)<1);
  helper->pev->flags&=~FL_DUCKING;helper->pev->origin=base+ystl::Vector (0,0,36);
  f.bot->pev->flags|=FL_ONGROUND;f.bot->pev->origin=base+ystl::Vector (0,0,108);f.bot->pev->groundentity=helper->Ent ();f.engine.AdvanceTime (0.1f);
  c=MakeCommand ();HumanCopycat::Finalize (*f.bot,c);CHECK (!(c.buttons&IN_JUMP));
  f.engine.AdvanceTime (0.13f);c=MakeCommand ();HumanCopycat::Finalize (*f.bot,c);CHECK (c.buttons&IN_JUMP);
  f.bot->pev->flags&=~FL_ONGROUND;f.bot->pev->groundentity=nullptr;f.bot->pev->origin=base+ystl::Vector (0,0,142);f.engine.AdvanceTime (0.05f);
  const auto origin=f.bot->pev->origin;c=MakeCommand ();HumanCopycat::Finalize (*f.bot,c);
  CHECK ((c.buttons&IN_DUCK)&&std::fabs (c.forward)+std::fabs (c.side)>50&&f.bot->pev->origin==origin&&f.bot->pev->velocity.empty ());
  f.bot->pev->flags=FL_CLIENT|FL_FAKECLIENT|FL_ONGROUND|FL_DUCKING;f.bot->pev->groundentity=f.engine.EdictList ();f.bot->pev->origin=landing+ystl::Vector (0,0,18);f.engine.AdvanceTime (0.1f);
  c=MakeCommand ();HumanCopycat::Finalize (*f.bot,c);CHECK (HumanArena::TestBoosts ()==1&&!HumanArena::Boosting (*f.bot)&&!HumanArena::Boosting (*helper));
  CHECK (HumanArena::VantageHolding (*f.bot)&&HumanArena::Camp (*f.bot));HumanCopycatHook::RunCamp (*f.bot);
  CHECK (HumanCopycatHook::TaskIdOf (*f.bot)==TaskId::Camp);
  cv_explore.Set (1);cv_explore_percent.Set (100);HumanExplore::NewRound ();c=MakeCommand ();c.forward=0;
  HumanCopycat::Finalize (*f.bot,c);CHECK (std::fabs (c.forward)+std::fabs (c.side)<1);
  f.engine.AdvanceTime (7);HumanCopycatHook::RunCamp (*f.bot);CHECK (!HumanArena::VantageHolding (*f.bot));
}
}

namespace bot {
namespace {
void EnableSmoke431 (Fixture &f) {
  EnableFinal (f); cv_human_objective_smoke.Set (1); cv_restricted_weapons.Set ("");
  f.engine.SetCvar ("sv_gravity",800.0f); f.engine.SetCvar ("mp_roundtime",3.0f); f.engine.SetCvar ("mp_freezetime",0.0f);
  game_state.RoundStart (); ExploreFloor (f); HumanArena::NewRound ();
  for (auto &b : bots) {
    const auto team = b.team_; HumanCopycatHook::Setup (b); b.team_ = team;
    HumanCopycatHook::OpenMovement (b); HumanCopycatHook::States (b,Sense::Invalid); HumanCopycatHook::Enemy (b,nullptr);
    b.pev->flags = FL_CLIENT | FL_FAKECLIENT | FL_ONGROUND; b.pev->health = 100; b.pev->deadflag = DEAD_NO;
  }
  f.bot->pev->origin = {100,0,0};
}
void Defuse431 (Fixture &f,float seconds) {
  f.bot->team_ = Team::CT; clients[f.bot->Ent ()].team = clients[f.bot->Ent ()].team2 = Team::CT;
  f.bot->has_defuser_ = false;
  f.engine.SetCvar ("mp_c4timer",seconds); game_state.SetBombPlanted (true); game_state.SetBombOrigin (false,{120,0,0});
  auto bomb = f.engine.SpawnEntity ("grenade"); bomb->v.origin = {120,0,0}; HumanCopycatHook::Pickup (*f.bot,bomb);
  HumanCopycatHook::BombTask (*f.bot,true);
}
}
TEST_CASE ("aim431/pit_sniper_watches_a_long_bombsite_a") {
  Fixture f; HOST_REQUIRE (f.Start ("de_pit431",true)); EnableFinal (f); EnableAim431 (f);
  f.bot->current_weapon_ = Weapon::AWP; f.bot->pev->weapons = 1<<18; f.bot->pev->origin = {0,0,-92};
  graph[1].origin = {1200,0,36}; graph[1].flags = ystl::to_underlying (NodeFlag::Goal);
  graph[2].origin = {0,1200,36}; graph[2].flags = ystl::to_underlying (NodeFlag::Goal); graph.PopulateNodes ();
  f.engine.SetTraceLineHook ([] (const float *a,const float *b,int flags,edict_t *skip,TraceResult *tr) {
    testhost::FakeEngine::TraceNoHit (a,b,flags,skip,tr);
    const bool a_long = b[0]-a[0]>900 && std::fabs (b[1]-a[1])<1 && b[2]-a[2]>80;
    if (!a_long) tr->flFraction = 0.2f;
  });
  HumanCopycatHook::CampTask (*f.bot); HOST_REQUIRE (HumanArena::Camp (*f.bot));
  ystl::Vector desired {0,90,0}; HumanArena::IdleLook (*f.bot,desired);
  CHECK (std::fabs (desired.y)<1 && desired.x < -5 && desired.x > -8);
  HumanCopycatHook::Look (*f.bot,{0,500,-92}); HumanAimCoach::Update (*f.bot,nullptr);
  auto cmd = MakeCommand (); HumanAimCoach::Prepare (*f.bot,cmd,desired,false,false,true,false);
  CHECK (std::fabs (desired.y)<1 && desired.x < -5); // navigation preaim must not flatten the uphill watch
  const auto before = f.bot->pev->origin; HumanCopycat::Finalize (*f.bot,cmd);
  CHECK (f.bot->pev->origin == before && f.bot->pev->velocity.empty ());
}
TEST_CASE ("aim431/plant_smoke_self_throw_resumes_real_objective") {
  Fixture f; HOST_REQUIRE (f.Start ("de_smoke431_plant",true)); EnableSmoke431 (f);
  f.bot->has_c4_ = true; f.bot->in_bomb_zone_ = true; f.bot->pev->weapons |= (1<<9)|(1<<6);
  HumanCopycatHook::BombTask (*f.bot,false); f.bot->pev->button = 0; HumanCopycatHook::Plant (*f.bot);
  HOST_REQUIRE (f.bot->GetTaskId () == TaskId::ThrowSmoke); CHECK (!(f.bot->pev->button & IN_ATTACK));
  const auto point = HumanCopycatHook::ThrowPoint (*f.bot); CHECK (point.distance (f.bot->pev->origin)<120);
  HumanCopycatHook::Smoke (*f.bot); CHECK (f.bot->GetTaskId () == TaskId::ThrowSmoke);
  auto grenade = f.engine.SpawnEntity ("grenade"); grenade->v.owner = f.bot->Ent ();
  grenade->v.model = string_t (f.engine.AllocString ("models/w_smokegrenade.mdl")); grenade->v.origin = point;
  grenade->v.velocity = {80,0,70}; const auto velocity = grenade->v.velocity;
  HumanCopycatHook::Smoke (*f.bot); CHECK (f.bot->GetTaskId () == TaskId::PlantBomb && grenade->v.velocity == velocity);
  f.engine.AdvanceTime (0.3f); game_state.UpdateActiveGrenade (); CHECK (!HumanCopycat::SmokeCover (f.bot->pev->origin));
  f.bot->pev->button = 0; HumanCopycatHook::Plant (*f.bot); CHECK (!(f.bot->pev->button & IN_ATTACK));
  game_state.TrackSmokeDetonation (grenade,point); HumanCopycat::Frame ();
  CHECK (HumanCopycat::SmokeCover (f.bot->pev->origin)); f.bot->pev->button = 0; HumanCopycatHook::Plant (*f.bot);
  CHECK (f.bot->GetTaskId () == TaskId::PlantBomb && (f.bot->pev->button & IN_ATTACK));
  CHECK (f.bot->pev->origin == ystl::Vector (100,0,0));
}
TEST_CASE ("aim431/defuse_smoke_prefers_teammate_and_keeps_progress") {
  Fixture f; HOST_REQUIRE (f.Start ("de_smoke431_defuse",true)); auto helper = DialogueBot (f,"Dosia",Team::CT); HOST_REQUIRE (helper);
  EnableSmoke431 (f); Defuse431 (f,35); HumanCopycatHook::OpenMovement (*helper); HumanCopycatHook::States (*helper,Sense::Invalid);
  helper->pev->origin = {350,0,0}; helper->pev->maxspeed = 250; helper->pev->weapons |= 1<<9; f.bot->pev->weapons |= 1<<9;
  f.bot->pev->button = 0; HumanCopycatHook::Defuse (*f.bot);
  CHECK (helper->GetTaskId () == TaskId::ThrowSmoke && f.bot->GetTaskId () == TaskId::DefuseBomb);
  CHECK (!(f.bot->pev->button & IN_USE));
  f.bot->has_progress_bar_ = true; f.bot->pev->button = 0; HumanCopycatHook::Defuse (*f.bot);
  CHECK ((f.bot->pev->button & IN_USE) && f.bot->GetTaskId () == TaskId::DefuseBomb);
  CHECK (HumanCopycatHook::ThrowPoint (*helper).distance (game_state.GetBombOrigin ())<120);
}
TEST_CASE ("aim431/urgent_defuse_never_waits_for_smoke") {
  Fixture f; HOST_REQUIRE (f.Start ("de_smoke431_urgent",true)); auto helper = DialogueBot (f,"Dosia",Team::CT); HOST_REQUIRE (helper);
  EnableSmoke431 (f); Defuse431 (f,14); HumanCopycatHook::OpenMovement (*helper); HumanCopycatHook::States (*helper,Sense::Invalid);
  helper->pev->origin = {350,0,0}; helper->pev->weapons |= 1<<9; f.bot->pev->weapons |= 1<<9;
  f.bot->pev->button = 0; HumanCopycatHook::Defuse (*f.bot);
  CHECK ((f.bot->pev->button & IN_USE) && f.bot->GetTaskId () == TaskId::DefuseBomb);
  CHECK (helper->GetTaskId () == TaskId::ThrowSmoke); // buddy may cover without delaying the worker
}
TEST_CASE ("aim431/failed_smoke_is_bounded_and_obeys_restrictions") {
  Fixture f; HOST_REQUIRE (f.Start ("de_smoke431_failure",true)); EnableSmoke431 (f);
  f.bot->has_c4_ = true; f.bot->in_bomb_zone_ = true; f.bot->pev->weapons |= (1<<9)|(1<<6);
  HumanCopycatHook::BombTask (*f.bot,false); HumanCopycatHook::Plant (*f.bot); HOST_REQUIRE (f.bot->GetTaskId () == TaskId::ThrowSmoke);
  // No projectile or detonation arrives: stop waiting and resume planting.
  f.engine.AdvanceTime (4.6f); HumanCopycatHook::Smoke (*f.bot); CHECK (f.bot->GetTaskId () == TaskId::PlantBomb);
  f.bot->pev->button = 0; HumanCopycatHook::Plant (*f.bot); CHECK (f.bot->pev->button & IN_ATTACK);
  CHECK (!HumanCopycat::SmokeCover (f.bot->pev->origin));
  HumanArena::NewRound (); HumanCopycatHook::OpenMovement (*f.bot); cv_restricted_weapons.Set (util.WeaponIdToAlias (Weapon::Smoke).chars ());
  CHECK (!HumanArena::ObjectiveSmoke (*f.bot,false));
  cv_restricted_weapons.Set (""); f.Wall (true); CHECK (!HumanArena::ObjectiveSmoke (*f.bot,false));
}
TEST_CASE ("aim431/objective_smoke_is_bought_with_real_budget") {
  Fixture f; HOST_REQUIRE (f.Start ("de_smoke431_buy",true)); EnableSmoke431 (f); PolicyCapture capture;
  f.bot->pev->weapons = 1<<17; f.bot->money_amount_ = 500; HumanCopycatHook::BuyNades (*f.bot);
  HOST_REQUIRE (policy_commands.size ()>=2); CHECK (policy_commands[0]=="buyequip" && policy_commands[1]=="menuselect 5");
  CHECK (policy_commands.size ()<=4); CHECK (!(f.bot->pev->weapons & (1<<9))); // no inventory grant
  policy_commands.clear (); f.bot->money_amount_ = 199; HumanCopycatHook::BuyNades (*f.bot); CHECK (policy_commands.empty ());
}
} // namespace bot

namespace bot {
TEST_CASE ("release432/runtime_identity_and_namespaces") {
  Fixture f; HOST_REQUIRE (f.Start ("de_aibo432_identity"));
  CHECK (product.name == "AiBo" && product.version == "4.3.3");
  CHECK (product.name_lower == "AiBo" && product.cmd_pri == "aibo" && product.cmd_sec == "ab");
  CHECK (std::string (::Plugin_info.name) == "AiBo" && std::string (::Plugin_info.version) == "4.3.3");
  CHECK (std::string (cv_human_aim_level.ptr->name) == "aibo_human_aim_level");
  CHECK (std::string (cv_quota.ptr->name) == "aibo_quota");
  CHECK (bstor.GetRunningPath ().find ("addons/AiBo") != ystl::String::InvalidIndex);
}
TEST_CASE ("release432/maximum_profile_stays_at_peak") {
  Fixture f; HOST_REQUIRE (f.Start ("de_aibo432_peak")); auto human = f.Foe (); EnableAim431 (f,100);
  auto cmd = MakeCommand (IN_ATTACK); HumanCopycatHook::Enemy (*f.bot,human);
  HumanCopycat::Finalize (*f.bot,cmd); CHECK (HumanAimCoach::Level (*f.bot) == Approx (1.0f));
  for (int i = 0; i < 10; ++i) { f.engine.AdvanceTime (1.0f); HumanAimCoach::Update (*f.bot,human); }
  CHECK (HumanAimCoach::Level (*f.bot) == Approx (1.0f));
  CHECK (cv_human_aim_techniques && cv_human_aim_wallbang && cv_human_aim_preaim && cv_human_aim_quickswitch && cv_human_aim_crouchhop);
}
} // namespace bot

namespace bot {
namespace {
void Enable433 (Fixture &f) {
  EnableFinal (f); EnableAim431 (f,100);
  cv_human_pro_profiles.Set (1); cv_human_hltv_skill.Set (1); cv_human_ct_patrol.Set (1);
  f.engine.SetCvar ("mp_roundtime",3.0f); f.engine.SetCvar ("mp_freezetime",0.0f); game_state.RoundStart ();
  for (auto &b : bots) {
    HumanCopycatHook::Setup (b); HumanCopycatHook::OpenMovement (b);
    HumanCopycatHook::States (b,Sense::Invalid); HumanCopycatHook::Enemy (b,nullptr);
    b.pev->flags=FL_CLIENT|FL_FAKECLIENT|FL_ONGROUND; b.pev->health=100; b.pev->deadflag=DEAD_NO;
  }
  f.bot->team_=Team::CT; clients[f.bot->Ent ()].team=clients[f.bot->Ent ()].team2=Team::CT;
  HumanArena::NewRound ();
}
bool Dust2Graph433 () {
  const auto root=std::filesystem::path (__FILE__).parent_path ().parent_path ().parent_path ().parent_path ();
  std::ifstream file (root/"cfg/addons/AiBo/data/graph/de_dust2.graph",std::ios::binary);
  StorageHeader h {}; file.read (reinterpret_cast<char *> (&h),sizeof h);
  if (!file || h.length!=1238 || h.uncompressed!=h.length*static_cast<int> (sizeof (Path)) || h.compressed<1) return false;
  std::vector<uint8_t> compressed (static_cast<size_t> (h.compressed)+16), raw (static_cast<size_t> (h.uncompressed)+16);
  file.read (reinterpret_cast<char *> (compressed.data ()),h.compressed); if(!file) return false;
  ystl::ULZ ulz;
  if(ulz.uncompress ({compressed.data (),static_cast<size_t> (h.compressed)},{raw.data (),static_cast<size_t> (h.uncompressed)})!=h.uncompressed) return false;
  graph.Reset ();
  for(int i=0;i<h.length;++i) {Path p {};std::memcpy (&p,raw.data ()+static_cast<size_t> (i)*sizeof (Path),sizeof (Path));graph.paths_.push (p);}
  graph.PopulateNodes (); planner.Init (); return true;
}
}
TEST_CASE ("release433/profile_roles_purchase_and_aliases") {
  Fixture f; HOST_REQUIRE (f.Start ("de_profile433")); Enable433 (f); cv_human_hltv_skill.Set (0);
  f.bot->money_amount_=16000; f.bot->pev->weapons=0; cv_restricted_weapons.Set ("");
  for(const auto &p:rbcc::proProfiles) {
    f.bot->pev->netname=string_t (f.engine.AllocString (p.name));
    for(auto team:{Team::CT,Team::Terrorist}) {
      f.bot->team_=team; cv_human_awp_percent.Set (p.weapon==rbcc::ProWeapon::AWP ? 0 : 100);
      CHECK (HumanCopycatHook::Primary (*f.bot)==(p.weapon==rbcc::ProWeapon::AWP ? Weapon::AWP : team==Team::CT ? Weapon::M4A1 : Weapon::AK47));
    }
  }
  for (auto alias:{"Kenny-S","Guardian (2)","PashaBiceps"}) {
    f.bot->pev->netname=string_t (f.engine.AllocString (alias)); CHECK (HumanCopycatHook::Primary (*f.bot)==Weapon::AWP);
  }
  f.bot->pev->netname=string_t (f.engine.AllocString ("Forest (2)")); CHECK (HumanCopycatHook::Primary (*f.bot)==Weapon::AK47);
  f.bot->pev->netname=string_t (f.engine.AllocString ("kennyS")); f.bot->money_amount_=3500;
  CHECK (HumanCopycatHook::Primary (*f.bot)==Weapon::AK47); // real economy, temporary fallback
  cv_restricted_weapons.Set ("awp"); f.bot->money_amount_=16000;CHECK (HumanCopycatHook::Primary (*f.bot)==Weapon::AK47);
}
TEST_CASE ("release433/rifler_touch_cannot_pick_up_awp") {
  Fixture f; HOST_REQUIRE (f.Start ("de_pickup433")); Enable433 (f);
  auto box=f.engine.SpawnEntity ("weaponbox");box->v.model=string_t (f.engine.AllocString ("models/w_awp.mdl"));
  f.bot->pev->netname=string_t (f.engine.AllocString ("NEO"));
  const int before=testhost::CsCalls (f.cs,"Touch");f.table.pfnTouch (box,f.bot->Ent ());
  CHECK (testhost::CsCalls (f.cs,"Touch")==before);
  f.bot->pev->netname=string_t (f.engine.AllocString ("GuardiaN"));f.table.pfnTouch (box,f.bot->Ent ());
  CHECK (testhost::CsCalls (f.cs,"Touch")==before+1);
}
TEST_CASE ("release433/hltv_rank_changes_ability_not_frags") {
  Fixture f; HOST_REQUIRE (f.Start ("de_rank433")); Enable433 (f); f.bot->pev->frags=7;
  f.bot->pev->netname=string_t (f.engine.AllocString ("NEO"));const auto neo=HumanAimCoach::Update (*f.bot,nullptr);
  f.bot->pev->netname=string_t (f.engine.AllocString ("f0rest"));const auto forest=HumanAimCoach::Update (*f.bot,nullptr);
  f.bot->pev->netname=string_t (f.engine.AllocString ("XANTARES"));const auto lower=HumanAimCoach::Update (*f.bot,nullptr);
  CHECK (neo>forest && forest>lower && lower>0.5f);CHECK (f.bot->pev->frags==Approx (7.0f));
  CHECK (rbcc::proProfile ("NEO")->rank==1 && rbcc::proProfile ("f0rest")->rank==2);
  CHECK (rbcc::rankPower (1)>rbcc::rankPower (10) && rbcc::rankPower (10)>rbcc::rankPower (20));
}
TEST_CASE ("release433/dust2_real_graph_hunts_all_three_rare_regions") {
  Fixture f; HOST_REQUIRE (f.Start ("de_dust2",true));Enable433 (f);HOST_REQUIRE (Dust2Graph433 ());
  f.bot->pev->origin={448,2464,-92};HumanCopycatHook::CurrentNode (*f.bot,0); cv_explore.Set (1);
  std::set<int> regions;
  for(int step=0;step<18;++step) {
    f.engine.AdvanceTime (2.1f);const int goal=HumanArena::Goal (*f.bot);HOST_REQUIRE (graph.Exists (goal));
    CHECK (HumanArena::PatrolActive (*f.bot) && !HumanExplore::Scout (*f.bot));
    const auto target=graph[goal].origin; const int region=HumanArena::PatrolRegion (target-ystl::Vector (0,0,36));
    CHECK (region>=0);regions.insert (region);
    f.bot->pev->origin=target;HumanCopycatHook::CurrentNode (*f.bot,goal);HumanArena::BeforeThink (*f.bot);
    HumanCopycatHook::Setup (*f.bot);f.bot->team_=Team::CT;HumanCopycatHook::OpenMovement (*f.bot);
  }
  CHECK (regions.size ()==3);
}
TEST_CASE ("release433/hunt_yields_to_contact_and_bomb_keeps_guards") {
  Fixture f; HOST_REQUIRE (f.Start ("de_dust2",true));auto buddy=DialogueBot (f,"TaZ",Team::CT);HOST_REQUIRE (buddy);
  Enable433 (f);buddy->team_=Team::CT;HOST_REQUIRE (Dust2Graph433 ());
  unsigned hunters=static_cast<unsigned> (HumanArena::PatrolDuty (*f.bot))+static_cast<unsigned> (HumanArena::PatrolDuty (*buddy));CHECK (hunters==1);
  f.bot->pev->origin={448,2464,-92};buddy->pev->origin=f.bot->pev->origin;
  HumanCopycatHook::CurrentNode (*f.bot,0);HumanCopycatHook::CurrentNode (*buddy,0);
  auto hunter=HumanArena::PatrolDuty (*f.bot) ? f.bot : buddy;
  HOST_REQUIRE (graph.Exists (HumanArena::PatrolGoal (*hunter)));
  HumanCopycatHook::States (*hunter,Sense::HearingEnemy); CHECK (!HumanArena::PatrolDuty (*hunter));
  HumanCopycatHook::States (*hunter,Sense::Invalid);game_state.SetBombPlanted (true);game_state.SetBombOrigin (false,{1200,2400,132});
  CHECK (HumanArena::PatrolGoal (*hunter)<0 && !HumanArena::PatrolActive (*hunter));
}
TEST_CASE ("release433/hunt_rejects_disconnected_region_nodes") {
  Fixture f; HOST_REQUIRE (f.Start ("de_dust2",true));Enable433 (f);
  graph[0].origin={448,2464,-92};graph[1].origin={1450,180,-156};graph[2].origin={-1600,1000,68};graph[3].origin={-300,-700,136};
  graph.PopulateNodes ();HumanCopycatHook::CurrentNode (*f.bot,0);f.bot->pev->origin=graph[0].origin;
  CHECK (HumanArena::PatrolGoal (*f.bot)<0); // endpoints exist but no connected route: no invented path
}
} // namespace bot
