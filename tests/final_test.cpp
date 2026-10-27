// GPL-2.0-or-later. Deterministic portable regression tests for FINAL 4.3.0.
#include "../src/copycat/copycat_final.h"
#include "../src/copycat/copycat_explore.h"
#include "../src/copycat/copycat_dialogue.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit (1); } } while (false)
using namespace rbcc;
namespace {
std::vector<uint8_t> read (const char *path) {
  std::ifstream f (path, std::ios::binary);
  return {std::istreambuf_iterator<char> (f), std::istreambuf_iterator<char> ()};
}
void write (const char *path, std::vector<uint8_t> bytes, bool checksum = true) {
  if (checksum && bytes.size () >= 4) {
    const auto h = hashBytes (bytes.data (), bytes.size () - 4);
    for (unsigned i = 0; i < 4; ++i) bytes[bytes.size () - 4 + i] = static_cast<uint8_t> (h >> (8*i));
  }
  std::ofstream f (path, std::ios::binary); f.write (reinterpret_cast<const char *> (bytes.data ()), bytes.size ());
}
bool utf8 (const std::string &s) {
  for (size_t i = 0; i < s.size ();) {
    const auto c = static_cast<unsigned char> (s[i]);
    const unsigned n = c < 128 ? 1 : (c >= 0xc2 && c <= 0xdf ? 2 : (c >= 0xe0 && c <= 0xef ? 3 : (c >= 0xf0 && c <= 0xf4 ? 4 : 0)));
    if (!n || i + n > s.size ()) return false;
    for (unsigned j = 1; j < n; ++j) if ((static_cast<unsigned char> (s[i+j]) & 0xc0) != 0x80) return false;
    i += n;
  }
  return true;
}
void memoryTest () {
  FinalMemory m; CHECK (m.fresh (1,{0,0,0}));
  m.remember (1,{0,0,0},PlaceUse::Camp);
  CHECK (!m.fresh (1,{170,0,0}) && m.fresh (1,{180,0,0}) && m.fresh (1,{0,0,80}));
  CHECK (m.fresh (2,{0,0,0}) && m.bias (1,{0,0,0}) < -600);
  m.remember (1,{10,0,0},PlaceUse::Hide); CHECK (m.sequence () == 1); // same stop is one decision
  m.strategy (1,FinalStrategy::Flank); CHECK (m.strategies (1,FinalStrategy::Flank) == 1);
  const char *path = "final-test.hfinal"; std::string error;
  CHECK (m.save (path,"de_final",123,error) && !m.dirty ());
  const auto good = read (path); CHECK (good.size () == 28 + 8 + 676);
  FinalMemory loaded; CHECK (loaded.load (path,"de_final",123,error));
  CHECK (!loaded.fresh (1,{0,0,0}) && loaded.strategies (1,FinalStrategy::Flank) == 1);
  CHECK (!loaded.load (path,"de_other",123,error) && !loaded.load (path,"de_final",124,error));
  CHECK (!loaded.fresh (1,{0,0,0})); // bad files leave the active memory intact
  auto corrupt = good; corrupt[30] ^= 0x80; write (path,corrupt,false);
  CHECK (!loaded.load (path,"de_final",123,error));
  for (size_t size : {size_t (0),size_t (27),good.size ()-1}) {
    write (path,{good.begin (),good.begin ()+size},false);
    CHECK (!loaded.load (path,"de_final",123,error));
  }
  // Checksummed but invalid NaN position must also be rejected transactionally.
  corrupt = good; const size_t pos = 24 + 8 + 36;
  corrupt[pos] = 0; corrupt[pos+1] = 0; corrupt[pos+2] = 0xc0; corrupt[pos+3] = 0x7f;
  write (path,corrupt); CHECK (!loaded.load (path,"de_final",123,error));
  CHECK (!loaded.fresh (1,{0,0,0}));
  for (unsigned i = 1; i <= 32; ++i) m.remember (1,{float (i)*300,0,0},PlaceUse::Patrol);
  CHECK (m.fresh (1,{0,0,0}) && m.bias (1,{0,0,0}) == 0);
  CHECK (!m.save ("missing-final-directory/memory.hfinal","de_final",123,error) && m.dirty ());
  for (unsigned i = 2; i <= 150; ++i) m.remember (i,{0,0,0},PlaceUse::Hide);
  CHECK (m.size () == FinalMemory::MaxProfiles);
  CHECK (m.save (path,"de_final",123,error) && loaded.load (path,"de_final",123,error));
  CHECK (loaded.size () == FinalMemory::MaxProfiles);
  std::remove (path);
}
void personaTest () {
  FinalPersona p; p.seed (123); auto last = p.current;
  std::set<unsigned> orders;
  for (int cycle = 0; cycle < 100; ++cycle) {
    std::set<FinalStrategy> bag; unsigned order = 0;
    for (int i = 0; i < 6; ++i) {
      const auto s = p.choose (); CHECK (s != last); bag.insert (s); last = s;
      order = order*6 + static_cast<unsigned> (s);
    }
    CHECK (bag.size () == 6); orders.insert (order);
  }
  CHECK (orders.size () > 50);
  FinalPersona q; q.seed (999); q.update (0,100,1,8,3); const auto risk = q.risk;
  q.hurt (80); q.update (0,20,1,8,3); CHECK (q.mood == FinalMood::Afraid && q.risk < risk);
  for (int i = 0; i < 8; ++i) q.killed (); q.update (0,100,15,15,1);
  CHECK (q.top && q.streak == 8 && q.risk > risk);
  q.update (0,100,15,18,2); CHECK (!q.top && q.rank == 2 && q.gap == 3);
  q.died (); q.died (); q.died (); q.update (0,100,15,18,2);
  CHECK (q.mood == FinalMood::Angry && q.deaths == 3 && q.streak == 0);
  const auto conf = q.confidence; q.killed (true); CHECK (q.confidence < conf);
  q.hurt (std::numeric_limits<float>::quiet_NaN ()); q.update (2,0,0,10,4);
  CHECK (std::isfinite (q.risk) && q.risk >= 0.08f && q.risk <= 0.95f);
  CHECK (finalCapacity (32,0) == 31 && finalCapacity (32,1) == 31 && finalCapacity (32,2) == 30);
  CHECK (finalCapacity (16,1) == 15 && finalCapacity (32,1,2) == 30);
  CHECK (finalCapacity (128,0) == 31 && finalCapacity (16,20) == 0 && finalCapacity (0,0) == 0);
}
void boostTest () {
  PhysicalBoost b; b.start (10); BoostObservation o;
  CHECK (b.update (10,o).phase == BoostPhase::Approach);
  o.helperReady = o.climberReady = true;
  auto in = b.update (10.1f,o); CHECK (in.phase == BoostPhase::Mount && in.helperDuck && in.jump);
  CHECK (!b.update (10.2f,o).jump);
  CHECK (b.update (10.7f,o).jump);
  CHECK (b.update (14.7f,o).phase == BoostPhase::Aborted); // collision disabled: no fabricated boost
  o = {}; b.start (20); o.helperReady = o.climberReady = true; b.update (20,o);
  o.supported = true; o.height = 36; in = b.update (20.1f,o);
  CHECK (in.phase == BoostPhase::Lift && in.helperDuck && !in.towardLanding);
  o.supported = false; o.height = 76; in = b.update (20.3f,o);
  CHECK (in.jump && !in.helperDuck && in.climberDuck && in.towardLanding);
  CHECK (b.update (20.8f,o).phase == BoostPhase::Lift); // time alone cannot count success
  o.landed = true; CHECK (b.update (21,o).phase == BoostPhase::Done);
  o.danger = true; CHECK (b.update (40,o).phase == BoostPhase::Done); // terminal state is stable
  b.start (30); CHECK (b.update (30,o).phase == BoostPhase::Aborted);
  o = {}; b.start (40); CHECK (b.update (39,o).phase == BoostPhase::Aborted);
  b.start (50); CHECK (b.update (54.1f,o).phase == BoostPhase::Aborted);
  b.start (60,128);o={};o.helperReady=o.climberReady=true;b.update (60,o);
  o.supported=true;o.height=36;b.update (60.1f,o);
  o.supported=false;o.height=78;auto first=b.update (60.3f,o);
  CHECK (!first.towardLanding&&!first.climberDuck&&!first.helperDuck);
  o.supported=true;o.helperStanding=true;o.height=72;auto mounted=b.update (60.5f,o);
  CHECK (!mounted.jump&&!mounted.towardLanding);
  CHECK (b.update (60.61f,o).jump);
  o.supported=false;o.height=106;auto upper=b.update (60.65f,o);
  CHECK (upper.climberDuck&&upper.towardLanding);
  o.landed=true;CHECK (b.update (60.8f,o).phase==BoostPhase::Done);
  b.start (70,150);CHECK (b.phase ()==BoostPhase::Aborted);
}
void dialogueTest () {
  const char *langs[] = {"en","id","sv","da","pl","fr","ru","uk","sk","pt","tr","ar","zh","ko","ja","de"};
  CHECK (static_cast<unsigned> (DialogueIntent::Count) == 23 && dialogueFragmentCount () == 1440);
  for (const auto *lang : langs) {
    DialogueMemory memory; DialogueContext c; c.frags = 15; c.rank = 2; c.topFragger = true; c.weapon = "AWP";
    uint32_t seed = 123; std::string text;
    for (unsigned i = 0; i < 690; ++i) {
      c.reply = i % 2; CHECK (composeDialogue (lang,static_cast<DialogueIntent> (i%23),c,seed,memory,text));
      CHECK (!text.empty () && text.size () <= 150 && utf8 (text));
      CHECK (text.find ('{') == std::string::npos && text.find ('\n') == std::string::npos);
    }
    c.topFragger = false; DialogueMemory a,b; uint32_t sa = 66, sb = sa; std::string ta,tb;
    CHECK (composeDialogue (lang,DialogueIntent::TopFragger,c,sa,a,ta));
    CHECK (composeDialogue (lang,DialogueIntent::Ambition,c,sb,b,tb)); CHECK (ta == tb);
    c.topFragger = true; seed = 123; CHECK (ambientIntent (c,DialogueIntent::Banter,seed) == DialogueIntent::TopFragger);
  }
}
void exploreTest () {
  ExploreMap m; m.seed ({0,0,0});
  auto probe = [] (const Vec &,const Vec &b,ExploreStep &s) {
    if (std::fabs (b.x) > 256 || std::fabs (b.y) > 256) return false;
    s.foot = b; return true;
  };
  for (int i = 0; i < 20; ++i) m.expand (32,probe);
  const auto target = m.nearest ({256,256,0}); CHECK (target >= 0);
  auto onlyFresh = [&] (const Vec &v) { return v.x > 240 && v.y > 240; };
  auto p = m.plan ({0,0,0},0,0,123,false,4096,{},onlyFresh);
  CHECK (p.goal == target && !p.path.empty ()); m.release (0);
  auto none = [] (const Vec &) { return false; };
  p = m.plan ({0,0,0},0,0,456,false,4096,{},none); CHECK (p.goal >= 0 && !p.path.empty ());
  m.release (0);
  auto bias = [] (const Vec &v) { return v.x > 240 && v.y > 240 ? 10000.0f : 0.0f; };
  p = m.plan ({0,0,0},0,0,999,false,4096,bias,{}); CHECK (p.goal == target);
}
}
int main () {
  memoryTest (); personaTest (); boostTest (); dialogueTest (); exploreTest ();
  std::cout << "PASS: persistent per-profile novelty, 600 nonrepeating strategy draws, moods/ranks, capacity, physical boost protocol, 11040 multilingual messages, fresh-first exploration\n";
}
