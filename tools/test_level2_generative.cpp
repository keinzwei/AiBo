// Level 2 Generative AI & 9-Bot Ultra-Light Cap Verification Suite for AiBo 4.4.1
#include "src/copycat/copycat_core.h"
#include "src/copycat/copycat_lite.h"
#include "src/copycat/copycat_learning.h"
#include "src/copycat/copycat_final.h"
#include "src/copycat/copycat_dialogue.h"
#include "src/copycat/copycat_explore.h"
#include "src/copycat/copycat_aim.h"
#include "src/copycat/copycat_routes.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>

using namespace rbcc;

static void testNineBotHardCap () {
  // Verify 31-bot max capacity across server configurations
  assert (finalCapacity (32, 0, 1) == 21);
  assert (finalCapacity (32, 1, 1) == 21);
  assert (finalCapacity (32, 2, 1) == 21);
  assert (finalCapacity (16, 1, 1) == 15);
  assert (desiredBots (31u, 32u, 1u) == 21u);
  assert (desiredBots (31u, 32u, 2u) == 21u);
}

static void testGraphAttentionExplore () {
  ExploreMap map;
  const int nStart  = map.seed (Vec (0.0f, 0.0f, 0.0f), 0);
  const int nHub    = map.seed (Vec (64.0f, 0.0f, 0.0f), 0);
  const int nAlcove = map.seed (Vec (128.0f, 64.0f, 0.0f), 1);
  const int nOpen   = map.seed (Vec (128.0f, -64.0f, 0.0f), 0);
  assert (nStart >= 0 && nHub >= 0 && nAlcove >= 0 && nOpen >= 0);

  map.annotate (nHub, 220, 10, 0, 0);
  map.annotate (nAlcove, 190, 230, 1, 90);
  map.annotate (nOpen, 80, 0, 0, 0);

  map.link (nStart, nHub, 0, true);
  map.link (nHub, nAlcove, 1, true);
  map.link (nHub, nOpen, 0, true);

  // Alcove tucked next to high-vantage hub should have strong GAT ambush score
  assert (map.nodes ()[static_cast<size_t> (nAlcove)].gat.ready);
  assert (map.gatScore (nAlcove, 0, false) > map.gatScore (nOpen, 0, false));

  // Online GAT reinforcement propagates reward to node and its 1-hop neighbor (nHub)
  const float hubBefore = map.nodes ()[static_cast<size_t> (nHub)].gat.learnedValue[0];
  for (int i = 0; i < 6; ++i) {
    map.reinforceNode (nAlcove, 0, 2.0f);
  }
  assert (map.gatUpdates () == 6);
  assert (map.nodes ()[static_cast<size_t> (nAlcove)].gat.learnedValue[0] > 1.0f);
  assert (map.nodes ()[static_cast<size_t> (nHub)].gat.learnedValue[0] > hubBefore);

  const auto plan = map.plan (Vec (0.0f, 0.0f, 0.0f), 0, 0, 12345u, false, 3072u);
  assert (!plan.path.empty ());
  assert (plan.goal == nAlcove);
}

static void testHierarchicalRlOptionCritic () {
  OnlineLearning learn;
  Frame f {};
  f.health = 90;
  f.maxSpeed = 250.0f;
  f.weapon = 28; // AK-47
  f.state = Ground;
  f.context.team = 0;
  f.context.phase = 1;
  f.context.visible = 1;
  f.context.enemyDistance = 300.0f;
  f.context.friends = 2;
  f.context.targetToken = 0x2002u;

  uint32_t seed = 77u;
  SubgoalOption opt = learn.chooseOption (f, SubgoalOption::HoldAngle, 5.0f, seed, 0.0f, true);
  assert (learn.hrl ().switches >= 1);

  // Train HRL Option-Critic to favor TradeSupport when friends >= 2
  Frame fNext = f;
  fNext.context.enemyDistance = 260.0f;
  for (int i = 0; i < 20; ++i) {
    learn.reinforceContinuous (f, LearnedAction::Right, 2.2f, fNext, false, false, AllLearningActions, true, SubgoalOption::TradeSupport);
  }
  assert (learn.hrl ().updates == 20);
  assert (learn.hrl ().optionValue[static_cast<size_t> (SubgoalOption::TradeSupport)] > 0.8f);

  const auto chosenOpt = learn.chooseOption (f, SubgoalOption::HoldAngle, 5.0f, seed, 0.0f, true);
  assert (chosenOpt == SubgoalOption::TradeSupport);

  const auto mod = learn.chooseContinuous (f, AllLearningActions, seed, 0.0f, true, true, chosenOpt);
  assert (mod.option == SubgoalOption::TradeSupport);

  // Verify HRL persistence in .hlearn2 (HCLRNML2)
  const std::string path = "/tmp/test_aibo_lvl2.hlearn";
  std::string err;
  assert (learn.save (path, "de_dust2", 0xCAFE1234u, err));
  OnlineLearning loaded;
  assert (loaded.load (path, "de_dust2", 0xCAFE1234u, err));
  assert (loaded.hrl ().updates == 20);
  assert (std::fabs (loaded.hrl ().optionValue[static_cast<size_t> (SubgoalOption::TradeSupport)] -
                     learn.hrl ().optionValue[static_cast<size_t> (SubgoalOption::TradeSupport)]) < 1e-4f);
  std::remove (path.c_str ());
  std::remove ((path + "2").c_str ());
}

static void testCausalAttentionAndDenoisingTrajectory () {
  SkillModel model;
  GenerativeMotion gen;
  gen.reset (0x55AA55AAu);

  Frame rec {};
  rec.msec = 20;
  rec.weapon = 28;
  rec.state = Ground;
  rec.maxSpeed = 250.0f;
  rec.forward = 180.0f;
  rec.side = 160.0f;
  Frame live = rec;

  for (int i = 0; i < 10; ++i) {
    const Frame adapted = gen.adaptReplayFrame (rec, live, Vec (600.0f, 200.0f, 0.0f), true, model, 0.020f, 0.75f);
    assert (std::isfinite (adapted.forward) && std::isfinite (adapted.side));
    assert (std::fabs (adapted.forward) <= 250.0f && std::fabs (adapted.side) <= 250.0f);
  }
  assert (gen.historyCount == GenerativeMotion::HistoryWindow);
  assert (gen.denoisedSteps == 10);
}

static void testAsyncCommanderAndGenerativeCallout () {
  FinalMemory mem;
  AsyncTacticalCommander cmd;
  TeamWorldSnapshot snap {};
  snap.team = 1; // CT
  snap.aliveAllies = 5;
  snap.aliveEnemies = 4;
  snap.avgMoney = 4800;
  snap.bombPlanted = true;
  snap.opponentAggression = 0.45f;
  snap.roundNumber = 3;
  snap.slots = {0, 1, 2, 3, 4};
  snap.snipers = {false, true, false, false, false};
  snap.carriers = {false, false, false, false, false};

  cmd.dispatch (snap, &mem);
  assert (cmd.dispatches () == 1);
  const auto &ord = cmd.order (1);
  assert (ord.sequence == 1);
  assert (ord.roles[1] == BotTacticalRole::SniperOverwatch);
  // Complementary crossfire angles assigned
  assert (std::fabs (ord.crossfireAngles[1]) > 10.0f);

  DialogueContext dctx {};
  dctx.money = 4800;
  dctx.health = 92;
  dctx.weapon = "M4A1";
  dctx.area = "Long-Pit";
  dctx.strategy = "flank";
  dctx.directive = teamDirectiveName (ord.directive);
  dctx.role = tacticalRoleName (ord.roles[0]);
  dctx.aliveAllies = 5;
  dctx.aliveEnemies = 4;

  DialogueMemory dmem {};
  uint32_t seed = 12u;
  std::string line;
  assert (composeDialogue ("id", DialogueIntent::Plan, dctx, seed, dmem, line));
  assert (!line.empty () && line.size () <= 150);
}

int main () {
  testNineBotHardCap ();
  testGraphAttentionExplore ();
  testHierarchicalRlOptionCritic ();
  testCausalAttentionAndDenoisingTrajectory ();
  testAsyncCommanderAndGenerativeCallout ();
  std::cout << "[PASS] All Level 2 Generative AI + 9-Bot Ultra-Light Cap tests passed!" << std::endl;
  return 0;
}
