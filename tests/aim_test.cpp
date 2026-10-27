// GPL-2.0-or-later. Adaptive skill, actual-shot cadence and fair physical limits.
#include "../src/copycat/copycat_aim.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#define CHECK(x) do { if (!(x)) {std::cerr<<__LINE__<<": "<<#x<<'\n';std::exit(1);} } while(false)
using namespace rbcc;
static void evidence () {
  AimEvidence weak, strong;
  for (int i = 0; i < 120; ++i) {
    weak.shot (8, false);
    if (i % 10 == 0)
      weak.hit ();
    strong.shot (0.3f, true);
    if (i % 3 != 0)
      strong.hit ();
    if (i % 10 == 0) {
      weak.reaction (850);
      strong.reaction (190);
      weak.duel (false);
      strong.duel (true, i % 20 == 0);
    }
  }
  CHECK (strong.confidence () == 1 && weak.confidence () == 1);
  CHECK (strong.estimate () > 0.85f && weak.estimate () < 0.15f);
  AimEvidence warm;
  for (int i = 0; i < 19; ++i)
    warm.shot (0, true);
  warm.reaction (200);
  warm.reaction (200);
  CHECK (warm.confidence () == 0);
  const auto before = strong.shots;
  strong.shot (std::numeric_limits<float>::quiet_NaN (), true);
  CHECK (strong.shots == before);
  strong.reaction (0);
  CHECK (strong.reactionMs >= 100);
  AdaptiveAimLevel level;
  level.update (2, 1, 1);
  CHECK (level.value > 0 && level.value <= 0.04001f);
  for (int i = 0; i < 100; ++i)
    level.update (1, strong.estimate (), 1);
  CHECK (level.value > 0.8f && level.value <= 1);
  const auto high = level.value;
  level.update (2, 0, 1);
  CHECK (high - level.value <= 0.01601f);
  level.update (std::numeric_limits<float>::quiet_NaN (), 0, 1);
  CHECK (std::isfinite (level.value));
}
static void knowledge () {
  AimKnowledge m;
  auto p = m.edit (42);
  CHECK (p);
  for (int i = 0; i < 80; ++i) {
    p->shot (0.4f, true);
    if (i % 2)
      p->hit ();
  }
  p->reaction (200);
  p->reaction (250);
  auto bot = m.edit (0x80000042u);
  CHECK (bot);
  bot->practiceSeconds = 3600;
  bot->shot (1, true);
  std::string err;
  const char *path = "aim-test.haim";
  CHECK (m.save (path, "de_aim", 555, err) && !m.dirty ());
  AimKnowledge copy;
  CHECK (copy.load (path, "de_aim", 555, err));
  CHECK (copy.get (42)->shots == 80 && copy.get (0x80000042u)->practiceSeconds == 3600);
  CHECK (!copy.load (path, "de_other", 555, err) && !copy.load (path, "de_aim", 556, err));
  CHECK (copy.get (42)->shots == 80);
  std::ifstream in (path, std::ios::binary);
  std::vector<uint8_t> bytes { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char> () };
  in.close ();
  auto bad = bytes;
  bad[30] ^= 1;
  {
    std::ofstream f (path, std::ios::binary);
    f.write (reinterpret_cast<const char *> (bad.data ()), bad.size ());
  }
  CHECK (!copy.load (path, "de_aim", 555, err));
  bad = bytes;
  const auto off = bad.size () - 8;
  bad[off] = 0;
  bad[off + 1] = 0;
  bad[off + 2] = 0xc0;
  bad[off + 3] = 0x7f;
  const auto checksum = hashBytes (bad.data (), bad.size () - 4);
  for (unsigned i = 0; i < 4; ++i)
    bad[bad.size () - 4 + i] = static_cast<uint8_t> (checksum >> (8 * i));
  {
    std::ofstream f (path, std::ios::binary);
    f.write (reinterpret_cast<const char *> (bad.data ()), bad.size ());
  }
  CHECK (!copy.load (path, "de_aim", 555, err));
  CHECK (copy.get (42)->shots == 80);
  {
    std::ofstream f (path, std::ios::binary);
    f.write ("HCAIM001", 8);
  }
  CHECK (!copy.load (path, "de_aim", 555, err));
  CHECK (!m.save ("missing-aim-directory/aim.haim", "de_aim", 555, err));
  CHECK (!m.edit (0));
  for (unsigned i = 1; i < 200; ++i)
    m.edit (i);
  CHECK (m.size () == AimKnowledge::MaxProfiles);
  CHECK (m.save (path, "de_aim", 555, err) && copy.load (path, "de_aim", 555, err));
  std::remove (path);
}
static void discipline () {
  std::set<unsigned> longBursts;
  for (unsigned i = 0; i < 99; ++i) {
    const auto p = shootingPlan (28, 1100, 1, AimStyle::Precision, i);
    if (p.mode == ShotMode::Burst)
      longBursts.insert (p.bullets);
  }
  CHECK (longBursts == std::set<unsigned> ({ 4, 5, 6 }));
  CHECK (shootingPlan (28, 1100, 0.5f, AimStyle::Precision, 0).mode == ShotMode::Tap);
  CHECK (shootingPlan (28, 500, 1, AimStyle::Spray, 1).mode == ShotMode::Spray);
  CHECK (shootingPlan (28, 500, 1, AimStyle::Precision, 1).mode == ShotMode::Burst);
  CHECK (shootingPlan (16, 1000, 1, AimStyle::Precision, 1).bullets == 1);
  ShotDiscipline s;
  auto plan = shootingPlan (28, 500, 1, AimStyle::Balanced, 3);
  CHECK (plan.bullets == 5);
  CHECK (s.allow (1, 9, 28, true, true, true, 0, 0, plan));
  for (unsigned i = 0; i < 5; ++i) {
    s.confirmed (1 + float (i) * 0.1f, 28, 1);
    CHECK (s.fired () == i + 1);
  }
  CHECK (!s.allow (1.5f, 9, 28, true, true, true, 0, 0, plan));
  CHECK (!s.allow (1.6f, 9, 28, true, true, true, 0, 0, plan));
  CHECK (s.allow (1.7f, 9, 28, true, true, true, 0, 0, plan));
  CHECK (s.fired () == 0);
  CHECK (!s.allow (1.72f, 9, 28, true, true, true, 200, 0, plan));
  CHECK (!s.allow (1.74f, 9, 28, true, false, true, 0, 0, plan));
  // Holding a button without engine ammo feedback never fabricates bullets.
  CHECK (s.fired () == 0);
  CHECK (!s.allow (2.6f, 9, 28, true, true, true, 0, 0, plan));
  s.reset ();
  const auto awp = shootingPlan (18, 1000, 1, AimStyle::Awper, 0);
  CHECK (!s.allow (3, 9, 18, true, true, false, 0, 0, awp));
  CHECK (s.allow (3.1f, 9, 18, true, true, true, 0, 0, awp));
  s.confirmed (3.1f, 18, 1);
  CHECK (!s.allow (3.3f, 9, 18, true, true, true, 0, 0, awp));
  auto pistol = shootingPlan (26, 100, 1, AimStyle::Balanced, 0);
  CHECK (s.allow (3.4f, 9, 26, true, true, true, 0, 0, pistol));
  CHECK (!s.allow (4.5f, 9, 18, true, true, true, 0, 0, awp));
  CHECK (s.allow (4.6f, 9, 18, true, true, true, 0, 0, awp));
  CHECK (plausibleWall (28, 'W', 24, 400) && !plausibleWall (28, 'M', 24, 400) && !plausibleWall (28, 'C', 24, 400));
  CHECK (!plausibleWall (16, 'W', 4, 300) && !plausibleWall (28, 'W', 100, 300) && !plausibleWall (28, 'W', 8, 6000));
}
static void aiming () {
  SkillModel baseline;
  const auto zero = expertModel (baseline, 0), high = expertModel (baseline, 1);
  CHECK (zero.reactionMs == baseline.reactionMs && zero.aimError == baseline.aimError && zero.turnRate == baseline.turnRate);
  CHECK (high.reactionMs <= baseline.reactionMs && high.aimError < baseline.aimError && high.turnRate >= baseline.turnRate);
  HumanAim low, pro;
  low.reset ({}, 29);
  pro.reset ({}, 29);
  float lowError = 0, proError = 0;
  unsigned imperfect = 0;
  for (int i = 0; i < 400; ++i) {
    const auto a = low.update (.025f, { 0, 20, 0 }, {}, 28, 1, true, 0, baseline, 700, 0, true, true);
    const auto b = pro.update (.025f, { 0, 20, 0 }, {}, 28, 1, true, 0, high, 700, 0, true, true, 1, true);
    if (i == 0)
      CHECK (std::fabs (b.angles.y) < 1 && !b.fire);
    if (i > 150) {
      lowError += std::fabs (angleDelta (20, a.angles.y));
      proError += std::fabs (angleDelta (20, b.angles.y));
      if (std::fabs (angleDelta (20, b.angles.y)) > 0.01f)
        ++imperfect;
    }
  }
  CHECK (proError < lowError && imperfect > 100);
}
int main () {
  evidence ();
  knowledge ();
  discipline ();
  aiming ();
  std::cout << "PASS: evidence-weighted skill, bounded adaptation, persistent human/bot practice, real-shot 1/2-5/4-6/spray cadence, AWP "
               "cooldown, penetration limits and fallible advanced aim\n";
}
