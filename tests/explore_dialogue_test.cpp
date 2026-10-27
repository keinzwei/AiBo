// GPL-2.0-or-later. Synthetic exploration topology and multilingual dialogue.
#include "../src/copycat/copycat_explore.h"
#include "../src/copycat/copycat_dialogue.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " << #x << '\n'; std::exit (1); } } while (false)
using namespace rbcc;
static bool utf8 (const std::string &s) {
  for (size_t i = 0; i < s.size ();) {
    const auto c = static_cast<unsigned char> (s[i]); unsigned width = c < 128 ? 1u : (c >= 0xc2 && c < 0xe0 ? 2u : (c < 0xf0 && c >= 0xe0 ? 3u : (c >= 0xf0 && c <= 0xf4 ? 4u : 0u)));
    if (!width || i + width > s.size ()) return false;
    for (unsigned j = 1; j < width; ++j) if ((static_cast<unsigned char> (s[i+j]) & 0xc0) != 0x80) return false;
    i += width;
  }
  return true;
}
int main () {
  ExploreMap m; CHECK (m.seed ({0,0,0}) == 0);
  // Two rooms connected only through a far tunnel. A lower pit is on the other side.
  auto free = [] (int x,int y) { return x >= 0 && x <= 8 && y >= 0 && y <= 8 && !(x == 4 && y != 6); };
  auto terrain = [] (int x,int y) { return x >= 7 && y <= 2 ? -32.0f : 0.0f; };
  auto probe = [&] (const Vec &a,const Vec &b,ExploreStep &s) {
    const int x = static_cast<int> (std::round (b.x / 64)), y = static_cast<int> (std::round (b.y / 64));
    const int ax = static_cast<int> (std::round (a.x / 64)), ay = static_cast<int> (std::round (a.y / 64));
    if (!free (x,y) || !free (ax,y) || !free (x,ay)) return false; // no diagonal wall cutting
    s.foot = {b.x,b.y,terrain (x,y)}; s.vantage = x >= 7 && y <= 2 ? 255 : 0;
    s.posture = y == 6 && x >= 3 && x <= 5 ? 1 : 0; return true;
  };
  for (unsigned i = 0; i < 100; ++i) m.expand (32,probe);
  CHECK (m.nodes ().size () == 73);
  CHECK (m.nearest ({8*64,0,-32}) >= 0); // pit, with no native GRAPH input
  CHECK (m.nearest ({4*64,6*64,0}) >= 0); // tunnel
  CHECK (m.nearest ({4*64,0,0}) < 0); // actual wall remains unmapped
  for (const auto &n : m.nodes ()) for (const auto &e : n.links) {
    ExploreStep s; CHECK (probe (n.foot,m.nodes ()[e.to].foot,s));
  }
  Vec current {}; std::set<int> physicallyVisited; bool traversedTunnel = false, reachedPit = false;
  for (unsigned trip = 0; trip < 180; ++trip) {
    auto plan = m.plan (current,0,0,trip*917u,false);
    CHECK (!plan.path.empty () && plan.examined <= ExploreMap::MaxNodes);
    for (auto n : plan.path) {
      physicallyVisited.insert (n); m.visit (n,0); current = m.nodes ()[n].foot;
      if (current.x == 256 && current.y == 384) traversedTunnel = true;
      if (current.x >= 448 && current.y <= 128 && current.z < 0) reachedPit = true;
    }
    m.release (0);
  }
  CHECK (physicallyVisited.size () == 73 && traversedTunnel && reachedPit);
  auto a = m.plan ({0,0,0},1,1,7,true); auto b = m.plan ({0,0,0},1,2,7,true);
  CHECK (a.goal >= 0 && b.goal >= 0 && a.goal != b.goal); // separate reservations
  CHECK (m.nodes ()[0].visits[1] == 0);
  const auto old = m.nodes ().size (); CHECK (m.seed ({0,0,80}) >= 0 && m.nodes ().size () == old + 1); // stacked floor
  CHECK (m.seed ({std::numeric_limits<float>::quiet_NaN (),0,0}) == -1);
  std::string error; CHECK (m.save ("explore-test.hexplore","de_test",123,error));
  ExploreMap loaded; CHECK (loaded.load ("explore-test.hexplore","de_test",123,error));
  CHECK (loaded.nodes ().size () == m.nodes ().size () && loaded.edges () == m.edges ());
  CHECK (!loaded.load ("explore-test.hexplore","de_other",123,error));
  CHECK (!loaded.load ("explore-test.hexplore","de_test",124,error));
  { std::fstream f ("explore-test.hexplore",std::ios::in|std::ios::out|std::ios::binary); f.seekp (60); f.put ('x'); }
  CHECK (!loaded.load ("explore-test.hexplore","de_test",123,error)); CHECK (loaded.nodes ().size () == m.nodes ().size ());
  m.visit (0,0); CHECK (!m.save ("missing-dir/explore.hexplore","de_test",123,error) && m.dirty ());
  std::remove ("explore-test.hexplore");
  CHECK (std::string (profileLanguage ("Hiko")) == "en"); CHECK (std::string (profileLanguage ("Dosia (2)")) == "ru");
  CHECK (std::string (profileLanguage ("f0rest")) == "sv"); CHECK (std::string (profileLanguage ("ISSAA")) == "ar");
  CHECK (std::string (profileLanguage ("xccurate")) == "id"); CHECK (std::string (profileLanguage ("Laz")) == "ja");
  CHECK (dialogueLanguageCount () == 16 && dialogueFragmentCount () == 1440);
  CHECK (parseIntent ("hello!") == DialogueIntent::Greeting && parseIntent ("привет") == DialogueIntent::Greeting);
  CHECK (parseIntent ("你好") == DialogueIntent::Greeting && parseIntent ("買い物") == DialogueIntent::Unknown);
  const char *languages[] = {"en","id","sv","da","pl","fr","ru","uk","sk","pt","tr","ar","zh","ko","ja","de"};
  for (const auto *lang : languages) {
    DialogueMemory memory; DialogueContext c; c.money = 2500; c.health = 28; c.weapon = "AWP";
    uint32_t seed = 19; std::set<std::string> messages; std::string text;
    for (unsigned i = 0; i < 600; ++i) {
      c.reply = i % 2 != 0;
      CHECK (composeDialogue (lang,static_cast<DialogueIntent> (i % 16),c,seed,memory,text));
      CHECK (text.size () <= 150 && utf8 (text)); CHECK (text.find ('{') == std::string::npos);
      CHECK (text.find ('\n') == std::string::npos && text.find ('\r') == std::string::npos);
      messages.insert (text);
    }
    CHECK (messages.size () >= 430);
    CHECK (composeDialogue (lang,DialogueIntent::Economy,c,seed,memory,text)); CHECK (text.find ("2500") != std::string::npos);
    CHECK (composeDialogue (lang,DialogueIntent::Health,c,seed,memory,text)); CHECK (text.find ("28") != std::string::npos);
    CHECK (composeDialogue (lang,DialogueIntent::Equipment,c,seed,memory,text)); CHECK (text.find ("AWP") != std::string::npos);
  }
  std::string russian = "привет команда"; truncateUtf8 (russian,9); CHECK (utf8 (russian) && russian.size () <= 9);
  std::cout << "PASS: all 73 reachable regions visited, tunnel/pit traversal, reservations, persistence, 16 languages and 9600 valid UTF-8 dialogue outputs\n";
}
