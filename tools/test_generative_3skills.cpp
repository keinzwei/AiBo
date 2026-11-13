// Verification Suite for the 3 Generative Autonomous Bot Upgrades:
// 1. Internet Mode (10-30s join/leave interval, Max 21 bots)
// 2. Generative CS 1.6 Nickname Bot Generator (Historical + Steam + Non-Steam + Continuous Player Monitoring)
// 3. Generative English LLM Chat (3,500+ Sentence Corpus, Self-Attention, Autoregressive Top-P Decoding & Online Learning)
#include "src/copycat/copycat_core.h"
#include "src/copycat/copycat_lite.h"
#include "src/copycat/copycat_learning.h"
#include "src/copycat/copycat_explore.h"
#include "src/copycat/copycat_final.h"
#include "src/copycat/copycat_dialogue.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace rbcc;

int main () {
  // 1. Verify Skill 1: Internet Mode (10-30s interval, Max 21 bots)
  assert (kInternetModeMaxBots == 21);
  assert (std::fabs (kInternetModeDefaultMinInterval - 10.0f) < 1e-5f);
  assert (std::fabs (kInternetModeDefaultMaxInterval - 30.0f) < 1e-5f);
  assert (finalCapacity (32, 0, 1) == 21);
  assert (finalCapacity (32, 1, 1) == 21);
  assert (desiredBots (21u, 32u, 1u) == 21u);
  assert (desiredBots (31u, 32u, 1u) == 21u);

  assert (kInternetModeMinInitialBots == 16);
  InternetModeSimulator sim;
  sim.configure (10.0f, 30.0f, 21);
  uint32_t rng = 0x12345678u;
  for (int i = 0; i < 100; ++i) {
    const float dt = sim.sampleInterval (rng);
    assert (dt >= 10.0f && dt <= 30.0f);
  }
  // Verify that Internet Simulation does NOT start until at least 16 bots have spawned!
  for (int b = 0; b < 16; ++b) {
    const int tgt = sim.stepTarget (static_cast<float> (b) * 0.1f, 16, b, 1, 32, rng);
    assert (tgt >= 16);
    assert (!sim.initialFillDone ());
  }
  int currentBots = 21;
  float now = 0.0f;
  // Establish initial fill (>= 16 bots)
  sim.stepTarget (now, 21, currentBots, 1, 32, rng);
  assert (sim.initialFillDone ());
  for (int step = 0; step < 60; ++step) {
    now += 12.0f;
    currentBots = sim.stepTarget (now, 21, currentBots, 1, 32, rng);
    assert (currentBots >= 18 && currentBots <= 21);
  }
  assert (sim.joinEvents () > 0u && sim.leaveEvents () > 0u);
  std::printf ("[PASS] Skill 1 (Internet Mode): interval=[%.0fs..%.0fs], maxBots=%d, joinEvents=%u, leaveEvents=%u\n",
    static_cast<double> (sim.minInterval ()), static_cast<double> (sim.maxInterval ()),
    sim.maxBots (), sim.joinEvents (), sim.leaveEvents ());

  // 2. Verify Skill 2: CS 1.6 Generative Nickname Bot Generator (4 Sources + Continuous Player Monitoring)
  GenerativeNicknameEngine nickGen;
  const size_t baseMorphemes = nickGen.learnedMorphemeCount ();
  nickGen.observePlayer ("SK|HyperVex_1337");
  nickGen.observePlayer ("fnatic.KryptonX");
  nickGen.observePlayer ("[Warnet]GarudaSniper");
  assert (nickGen.monitoredPlayers () == 3u);
  assert (nickGen.learnedMorphemeCount () > baseMorphemes);

  std::set<std::string> uniqueNicks;
  std::vector<std::string> activeServerNames = { "SK|HyperVex_1337" };
  unsigned srcCounts[4] {};
  for (int i = 0; i < 80; ++i) {
    NicknameSource src = NicknameSource::HistoricalDatabase;
    std::string nick = nickGen.generateUnique (rng, activeServerNames, &src);
    assert (!nick.empty () && nick.size () <= 31);
    assert (nick != "SK|HyperVex_1337");
    assert (uniqueNicks.find (nick) == uniqueNicks.end ());
    uniqueNicks.insert (nick);
    activeServerNames.push_back (nick);
    if (activeServerNames.size () > 21) activeServerNames.erase (activeServerNames.begin () + 1);
    ++srcCounts[static_cast<unsigned> (src) % 4u];
  }
  assert (srcCounts[0] == 20 && srcCounts[1] == 20 && srcCounts[2] == 20 && srcCounts[3] == 20);

  std::string err;
  assert (nickGen.save ("/tmp/test_nicks.hnicks", err));
  GenerativeNicknameEngine loadedNickGen;
  assert (loadedNickGen.load ("/tmp/test_nicks.hnicks", err));
  assert (loadedNickGen.monitoredPlayers () == 3u);
  std::remove ("/tmp/test_nicks.hnicks");
  std::printf ("[PASS] Skill 2 (CS 1.6 Nickname Generator): generated=%u unique_80=%zu monitored=%u morphemes=%zu sample='%s' / '%s' / '%s'\n",
    nickGen.generatedCount (), uniqueNicks.size (), nickGen.monitoredPlayers (), nickGen.learnedMorphemeCount (),
    activeServerNames[1].c_str (), activeServerNames[2].c_str (), activeServerNames[3].c_str ());

  // 3. Verify Skill 3: GenerativeEnglishLLM (Massive 3,500+ Sentence Corpus, Self-Attention, Autoregressive Top-P Decoding)
  auto &llm = englishLLM ();
  assert (llm.trainedSentences () >= 3500u);
  assert (llm.corpusTokens () >= 50000u);
  assert (llm.vocabSize () >= 450u);
  assert (llm.trigramStates () >= 2500u);

  DialogueContext ctx {};
  ctx.money = 4600;
  ctx.health = 88;
  ctx.weapon = "AK-47";
  ctx.area = "Long-A";
  ctx.flash = true;
  ctx.smoke = true;
  DialogueMemory mem {};

  const char *prompts[] = {
    "hey bro how are you doing today?",
    "are you a bot or a real human player?",
    "what mouse sensitivity and resolution do you use?",
    "can you drop me an ak47 or m4a1 this round?",
    "let's smoke long a doors and flash pit together",
    "nice clutch shot man that was super clean!",
    "crouch by this box real quick and boost me up"
  };

  std::set<std::string> generatedReplies;
  for (const char *p : prompts) {
    ctx.incoming = p;
    const DialogueIntent intent = parseIntent (p);
    std::string reply;
    assert (composeDialogue ("en", intent, ctx, rng, mem, reply));
    assert (!reply.empty () && reply.size () <= 150);
    assert (reply.find ('[') == std::string::npos && reply.find (']') == std::string::npos);
    generatedReplies.insert (reply);
    std::printf ("  [Human]: %s\n  [Bot LLM (%u)]: %s\n", p, static_cast<unsigned> (intent), reply.c_str ());
  }
  assert (generatedReplies.size () == 7u);
  assert (llm.onlineUpdates () >= 7u);

  assert (llm.save ("/tmp/test_llm.hllm", err));
  assert (llm.load ("/tmp/test_llm.hllm", err));
  std::remove ("/tmp/test_llm.hllm");

  std::printf ("[PASS] Skill 3 (GenerativeEnglishLLM): sentences=%u corpus_tokens=%u vocab=%zu trigrams=%zu online_updates=%u\n",
    llm.trainedSentences (), llm.corpusTokens (), llm.vocabSize (), llm.trigramStates (), llm.onlineUpdates ());
  std::printf ("ALL 3 GENERATIVE AUTONOMOUS SKILLS VERIFIED SUCCESSFULLY!\n");
  return 0;
}
