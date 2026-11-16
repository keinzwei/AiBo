// Level 1 Generative & Continuous AI Verification Suite for AiBo 4.3.5
#include "src/copycat/copycat_core.h"
#include "src/copycat/copycat_learning.h"
#include "src/copycat/copycat_final.h"
#include "src/copycat/copycat_dialogue.h"
#include "src/copycat/copycat_explore.h"
#include "src/copycat/copycat_aim.h"
#include "src/copycat/copycat_routes.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

using namespace rbcc;

static void testTinyMlpAndOpponentProfiler () {
  OnlineLearning learn;
  Frame f1 {};
  f1.health = 85;
  f1.maxSpeed = 250.0f;
  f1.weapon = 28; // AK-47
  f1.state = Ground;
  f1.context.team = 0;
  f1.context.phase = 1;
  f1.context.visible = 1;
  f1.context.enemyDistance = 320.0f;
  f1.context.enemyYaw = 6.0f;
  f1.context.friends = 1;
  f1.context.targetToken = 0x1001u;

  // 1. Observe individual opponent #1 as a stationary AWP holder
  for (int i = 0; i < 8; ++i) {
    learn.observePlayer (0x1001u, 1, 0.02f, 5.0f, true, 18, Vec (500.0f, 200.0f, 0.0f));
  }
  learn.recordDuel (0x1001u, false);
  const auto *prof = learn.playerProfile (0x1001u);
  assert (prof != nullptr);
  assert (prof->sightings == 8);
  assert (prof->awpPreference > 0.7f);
  assert (prof->stationary > 0.7f);
  assert (prof->duelWinRate > 0.5f);
  assert (learn.trackedPlayers () == 1);

  // 2. Train continuous imitation & reinforcement learning on TinyMLP
  f1.side = 210.0f;
  f1.buttons = Duck;
  for (int i = 0; i < 15; ++i) {
    learn.imitateContinuous (f1, LearnedAction::Right, true);
  }
  Frame f2 = f1;
  f2.context.enemyDistance = 290.0f;
  for (int i = 0; i < 25; ++i) {
    learn.reinforceContinuous (f1, LearnedAction::Right, 1.8f, f2, false, false, AllLearningActions, true);
    learn.reinforceContinuous (f1, LearnedAction::Hold, -1.2f, f2, false, false, AllLearningActions, true);
  }
  assert (learn.mlp ().steps >= 65);
  assert (learn.stats ().updates == 50);
  assert (learn.stats ().humanSamples == 15);

  // 3. Verify generalization to a slightly different continuous state (unvisited discrete state)
  Frame fUnseen = f1;
  fUnseen.health = 32; // Different discrete health bucket!
  fUnseen.context.enemyDistance = 340.0f;
  uint32_t seed = 42u;
  const auto mod = learn.chooseContinuous (fUnseen, AllLearningActions, seed, 0.0f, true, true);
  assert (mod.action == LearnedAction::Right);
  assert (mod.strafeScale >= 0.50f && mod.strafeScale <= 1.00f);
  assert (mod.crouchLikelihood > 0.15f);

  // 4. Verify persistence (.hlearn + .hlearn2)
  const std::string path = "/tmp/test_aibo_lvl1.hlearn";
  std::string err;
  assert (learn.save (path, "de_dust2", 0x12345678u, err));
  assert (err.empty ());

  OnlineLearning loaded;
  assert (loaded.load (path, "de_dust2", 0x12345678u, err));
  assert (loaded.mlp ().steps == learn.mlp ().steps);
  assert (loaded.trackedPlayers () == 1);
  const auto *loadedProf = loaded.playerProfile (0x1001u);
  assert (loadedProf != nullptr);
  assert (std::fabs (loadedProf->awpPreference - prof->awpPreference) < 1e-4f);
  std::remove (path.c_str ());
  std::remove ((path + "2").c_str ());
}

static void testContextualUtilityAndHeatmap () {
  FinalMemory mem;
  FinalPersona persona;
  persona.seed (0xCAFEBABEu);

  // 1. Sniper context should strongly prefer Snipe strategy
  UtilityContext awpCtx {};
  awpCtx.healthRatio = 1.0f;
  awpCtx.economyRatio = 0.8f;
  awpCtx.isSniper = 1.0f;
  awpCtx.personalRisk = 0.35f;
  const auto s1 = persona.chooseUtility (awpCtx, &mem, 101u);
  assert (s1 == FinalStrategy::Snipe);

  // 2. Outnumbered low-HP context rewarded for Lurk should shift utility toward Lurk/Support
  UtilityContext clutchCtx {};
  clutchCtx.healthRatio = 0.30f;
  clutchCtx.numericalAdvantage = -0.60f;
  clutchCtx.isSniper = 0.0f;
  clutchCtx.opponentAggression = 0.65f;
  persona.current = FinalStrategy::Lurk;
  persona.lastContext = clutchCtx;
  for (int i = 0; i < 12; ++i) {
    persona.rewardStrategy (1.5f);
    mem.recordStrategyOutcome (101u, FinalStrategy::Lurk, 1.5f);
  }
  assert (persona.evaluateStrategy (FinalStrategy::Lurk, clutchCtx, &mem, 101u) >
          persona.evaluateStrategy (FinalStrategy::Push, clutchCtx, &mem, 101u));

  // 3. Spatial Threat & Opportunity Heatmap
  const Vec chokePoint (100.0f, 200.0f, 0.0f);
  const Vec flankRoute (-400.0f, 600.0f, 0.0f);
  mem.recordSpatialEvent (chokePoint, 4.5f, 0.2f); // High danger choke point
  mem.recordSpatialEvent (flankRoute, 0.1f, 3.8f); // High opportunity flank
  assert (mem.threatAt (chokePoint) > 3.0f);
  assert (mem.opportunityAt (flankRoute) > 3.0f);

  const float chokeBias = mem.tacticalBias (101u, chokePoint, FinalStrategy::Flank, 0.4f);
  const float flankBias = mem.tacticalBias (101u, flankRoute, FinalStrategy::Flank, 0.4f);
  assert (flankBias > chokeBias + 500.0f);

  // 4. Verify persistence (.hfinal + .hfinal2)
  mem.remember (101u, flankRoute, PlaceUse::Patrol);
  const std::string path = "/tmp/test_aibo_lvl1.hfinal";
  std::string err;
  assert (mem.save (path, "de_dust2", 0x87654321u, err));
  FinalMemory loaded;
  assert (loaded.load (path, "de_dust2", 0x87654321u, err));
  assert (loaded.heatmapSize () == 2);
  assert (std::fabs (loaded.strategyValue (101u, FinalStrategy::Lurk) - mem.strategyValue (101u, FinalStrategy::Lurk)) < 1e-4f);
  std::remove (path.c_str ());
  std::remove ((path + "2").c_str ());
}

static void testGenerativeMotionAndSkillModel () {
  SkillModel model;
  Clip clip;
  clip.id = 1;
  clip.profile = 7;
  for (int i = 0; i < 20; ++i) {
    Frame f;
    f.msec = 25;
    f.weapon = 28;
    f.state = Ground;
    f.maxSpeed = 250.0f;
    f.origin = Vec (static_cast<float> (i) * 4.0f, 0.0f, 0.0f);
    if (i > 0 && i < 19) {
      f.context.visible = 1;
      f.context.enemyDistance = 400.0f;
      f.side = (i < 10) ? 200.0f : -200.0f;
    }
    clip.frames.push_back (f);
    clip.durationMs += f.msec;
  }
  clip.end = clip.frames.back ().origin;
  clip.features = analyze (clip.frames);
  model.learn (clip);
  assert (model.strafeSamples > 0);
  assert (model.strafeCadenceMs >= 120.0f && model.strafeCadenceMs <= 750.0f);

  GenerativeMotion gen1, gen2;
  gen1.reset (111u);
  gen2.reset (222u);
  Frame live = clip.frames[5];
  const Vec goal (500.0f, 300.0f, 0.0f);
  const Frame out1 = gen1.adaptReplayFrame (clip.frames[5], live, goal, true, model, 0.025f, 0.8f);
  const Frame out2 = gen2.adaptReplayFrame (clip.frames[5], live, goal, true, model, 0.025f, 0.8f);
  // Two different seeds produce distinct, organic trajectories while staying within valid bounds
  assert (std::fabs (out1.side - out2.side) > 0.01f || std::fabs (out1.forward - out2.forward) > 0.01f);
  assert (std::fabs (out1.side) <= 250.0f && std::fabs (out1.forward) <= 250.0f);
}

static void testEpisodicDialogue () {
  DialogueContext ctx {};
  ctx.money = 4500;
  ctx.health = 28;
  ctx.weapon = "AK-47";
  ctx.opponent = "s1mple";
  ctx.area = "Long-Pit";
  ctx.strategy = "flank";
  ctx.aliveAllies = 1;
  ctx.aliveEnemies = 3;
  ctx.clutch = true;
  ctx.revenge = true;
  ctx.headshot = true;

  DialogueMemory mem {};
  uint32_t seed = 99u;
  std::string out;
  assert (composeDialogue ("id", DialogueIntent::Kill, ctx, seed, mem, out));
  assert (!out.empty () && out.size () <= 150);
  assert (out.find ('[') == std::string::npos && out.find (']') == std::string::npos);

  std::string planOut;
  assert (composeDialogue ("en", DialogueIntent::Plan, ctx, seed, mem, planOut));
  assert (!planOut.empty () && planOut.size () <= 150);
  assert (planOut.find ('[') == std::string::npos && planOut.find (']') == std::string::npos);
}

int main () {
  testTinyMlpAndOpponentProfiler ();
  testContextualUtilityAndHeatmap ();
  testGenerativeMotionAndSkillModel ();
  testEpisodicDialogue ();
  std::cout << "[PASS] All Level 1 Generative AI tests passed!" << std::endl;
  return 0;
}
