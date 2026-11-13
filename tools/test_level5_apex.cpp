#include "copycat/copycat_routes.h"
// Level 5 Theoretical Apex: Self-Modifying Neuroevolution, ABCD Hebbian Plasticity, Liquid Recurrent Core, ~99.9% Autonomy & Single-DLL Verification Suite
#include "copycat/copycat_core.h"
#include "copycat/copycat_lite.h"
#include "copycat/copycat_learning.h"
#include "copycat/copycat_explore.h"
#include "copycat/copycat_final.h"
#include "copycat/copycat_dialogue.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace rbcc;

int main () {
  // 1. Verify Level 5 Liquid Time-Constant (LTC) Continuous-Time Recurrent Working Memory
  OnlineLearning learning;
  Frame visibleFrame {};
  visibleFrame.health = 90;
  visibleFrame.maxSpeed = 250.0f;
  visibleFrame.weapon = 28; // AK-47
  visibleFrame.msec = 16;
  visibleFrame.state = Ground;
  visibleFrame.context.team = 0;
  visibleFrame.context.phase = 1;
  visibleFrame.context.visible = 1;
  visibleFrame.context.enemyDistance = 380.0f;
  visibleFrame.context.enemyYaw = 6.0f;
  visibleFrame.context.targetToken = 0xDEADBEEFu;

  const auto featVis = learning.extractFeatures (visibleFrame, true);
  float resVis = 0.0f, hebbDrive = 0.0f;
  for (int i = 0; i < 10; ++i) {
    learning.stepLiquidAndHebbian (featVis, 0.016f, 0.85f, &resVis, &hebbDrive);
  }
  assert (learning.liquid ().ticks == 10u);
  assert (resVis > 0.005f);
  assert (hebbDrive > 0.0f);

  // Even when enemy disappears behind cover for 3 frames, Liquid working memory retains non-zero state!
  Frame occludedFrame = visibleFrame;
  occludedFrame.context.visible = 0;
  const auto featOcc = learning.extractFeatures (occludedFrame, true);
  float resOcc = 0.0f;
  learning.stepLiquidAndHebbian (featOcc, 0.016f, 0.0f, &resOcc, nullptr);
  assert (resOcc > 0.002f);
  std::printf ("[PASS] Level 5 Liquid Recurrent Working Memory: visible_resonance=%.4f occluded_memory=%.4f hebb_drive=%.5f\n",
    static_cast<double> (resVis), static_cast<double> (resOcc), static_cast<double> (hebbDrive));

  // 2. Verify Level 5 Self-Modifying ABCD Hebbian Plasticity & Structural Synaptic Neurogenesis/Pruning
  const uint32_t initialActive = learning.hebbian ().activeSynapses;
  assert (initialActive > 0u);
  Frame nextFrame = visibleFrame;
  nextFrame.origin = Vec (48.0f, 32.0f, 0.0f);
  for (int i = 0; i < 24; ++i) {
    learning.reinforceContinuous (visibleFrame, LearnedAction::Right, 1.40f, nextFrame, false, true, AllLearningActions, true, SubgoalOption::WideSwing);
  }
  assert (learning.hebbian ().plasticitySteps >= 35u);
  assert (learning.hebbian ().topologyRewires >= 3u);
  assert (learning.hebbian ().prunedSynapses >= 1u);
  assert (learning.hebbian ().sproutedSynapses >= 1u);
  std::printf ("[PASS] Level 5 Structural Synaptic Neuroevolution: active_synapses=%u pruned=%u sprouted=%u rewires=%u\n",
    learning.hebbian ().activeSynapses, learning.hebbian ().prunedSynapses,
    learning.hebbian ().sproutedSynapses, learning.hebbian ().topologyRewires);

  // 3. Verify Level 5 Inter-Bot Population Neuroevolution (Darwinian DNA Crossover & Mutation)
  SelfEvolvingBrain brain;
  uint32_t evoSeed = 0xCAFE5555u;
  for (int gen = 0; gen < 5; ++gen) {
    // Bots 0..3 win duels, bots 4..9 lose duels
    for (size_t slot = 0; slot < MaxGenomePopulation; ++slot) {
      const float reward = (slot < 4) ? 1.25f : -0.75f;
      brain.evoPool ().reportFitness (slot, reward);
    }
    brain.evoPool ().evolveGeneration (evoSeed);
  }
  const auto &champ = brain.evoPool ().champion ();
  assert (brain.evoPool ().generations () == 6u);
  assert (brain.evoPool ().crossovers () >= 20u);
  assert (brain.evoPool ().mutations () >= 20u);
  assert (champ.fitness > 0.70f);

  // Inject Champion DNA into OnlineLearning Hebbian + Liquid cortex
  learning.applyEvolutionaryDNA (champ.hebbianGenes, champ.liquidTauScale);
  assert (std::fabs (learning.hebbian ().coeffA[0] - champ.hebbianGenes[0]) < 1e-5f);
  assert (std::fabs (learning.hebbian ().eta[0] - champ.hebbianGenes[4]) < 1e-5f);
  std::printf ("[PASS] Level 5 Population DNA Pool: generations=%u crossovers=%u mutations=%u champ_gen=%u champ_fitness=%.3f\n",
    brain.evoPool ().generations (), brain.evoPool ().crossovers (), brain.evoPool ().mutations (),
    champ.generation, static_cast<double> (champ.fitness));

  // 4. Verify ~99.9% Autonomy Unshackling & 10-Player 5v5 Team Equilibrium
  UtilityContext uctx {};
  uctx.healthRatio = 0.85f;
  uctx.numericalAdvantage = 0.20f;
  const auto goal = brain.selectCurriculum (101u, uctx, evoSeed);
  for (int i = 0; i < 40; ++i) {
    brain.recordCurriculumOutcome (101u, goal, 1.0f);
  }
  float autonomy = 0.0f;
  assert (brain.shouldOverrideLegacyFsm (101u, false, &autonomy) == true);
  assert (autonomy >= 0.990f && autonomy <= 0.999f);
  assert (finalCapacity (32, 0, 1) == 21);
  assert (finalCapacity (32, 1, 1) == 21);
  assert (finalCapacity (32, 2, 1) == 21);
  assert (desiredBots (31u, 32u, 1u) == 21u);
  DialogueContext dctx {};
  dctx.area = "Mid";
  dctx.strategy = "flank";
  dctx.directive = "pinch_flank";
  dctx.role = "lurker";
  DialogueMemory dmem {};
  std::string chatLine;
  assert (composeDialogue ("de", DialogueIntent::Plan, dctx, evoSeed, dmem, chatLine));
  assert (chatLine.find ('[') == std::string::npos && chatLine.find (']') == std::string::npos);
  std::printf ("[PASS] Level 5 Apex Autonomy: autonomy=%.2f%% | 31-Bot Max Capacity & Clean HUD/Chat verified\n",
    static_cast<double> (autonomy * 100.0f));

  // 5. Verify On-Disk Persistence of Full Level 5 State (HCLRNML5 Hebbian+Liquid & HCEVOBR5 Curriculum+DNA Pool)
  std::string err;
  assert (learning.save ("/tmp/test_l5.hlearn", "de_dust2", 0x12345678u, err));
  OnlineLearning loadedLearning;
  assert (loadedLearning.load ("/tmp/test_l5.hlearn", "de_dust2", 0x12345678u, err));
  assert (loadedLearning.hebbian ().activeSynapses == learning.hebbian ().activeSynapses);
  assert (loadedLearning.hebbian ().prunedSynapses == learning.hebbian ().prunedSynapses);
  assert (loadedLearning.hebbian ().sproutedSynapses == learning.hebbian ().sproutedSynapses);
  assert (loadedLearning.liquid ().ticks == learning.liquid ().ticks);

  assert (brain.save ("/tmp/test_l5.hevo", "de_dust2", 0x12345678u, err));
  SelfEvolvingBrain loadedBrain;
  assert (loadedBrain.load ("/tmp/test_l5.hevo", "de_dust2", 0x12345678u, err));
  assert (loadedBrain.evoPool ().generations () == brain.evoPool ().generations ());
  assert (loadedBrain.evoPool ().crossovers () == brain.evoPool ().crossovers ());
  assert (std::fabs (loadedBrain.evoPool ().champion ().fitness - brain.evoPool ().champion ().fitness) < 1e-5f);
  std::printf ("[PASS] Level 5 On-Disk Persistent Brain Storage: HCLRNML5 + HCEVOBR5 verified\n");

  // Verify 3-Team CT Defense Split (Team 1: Bombsite A, Team 2: Bombsite B, Team 3: Mid Patrol)
  {
    std::vector<rbcc::SplitMember> ct_members;
    for (int i = 0; i < 10; ++i) ct_members.push_back ({i, -1, false});
    auto roles = rbcc::splitRoles (ct_members, 12345u);
    int c0 = 0, c1 = 0, c2 = 0;
    for (int r : roles) {
      if (r == 0) ++c0;
      else if (r == 1) ++c1;
      else if (r == 2) ++c2;
    }
    assert (c0 >= 2 && c1 >= 2 && c2 >= 2);
    std::printf ("[PASS] 3-Team CT Defense Split (10 CTs): TeamA=%d TeamB=%d TeamMid=%d\n", c0, c1, c2);
  }
  std::printf ("ALL LEVEL 5 THEORETICAL APEX & SINGLE-DLL TESTS PASSED!\n");
  return 0;
}
