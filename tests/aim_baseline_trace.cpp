#include "copycat_core.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
using namespace rbcc;
int main () {
  HumanAim a;
  SkillModel m;
  m.turnRate = std::clamp (m.turnRate, 60.f, 480.f);
  m.aimError = std::clamp (m.aimError, 0.25f, 5.f);
  m.reactionMs = std::clamp (m.reactionMs * .95f, 140.f, 900.f);
  for (unsigned i = 0; i < 31; ++i) {
    m.recoil[i] = .72f;
    m.recoilSamples[i] = 1;
  }
  a.reset ({ 0, 179, 0 }, 1981);
  const unsigned guns[] = { 28, 22, 18, 16, 26 };
  for (int i = 0; i < 1200; ++i) {
    const unsigned w = guns[(i / 240) % 5];
    const float t = float (i) * .025f;
    const auto r = a.update (.025f, { std::sin (t) * 12, std::cos (t * .6f) * 160, 0 }, { std::sin (t) * 2, std::cos (t) * .5f, 0 }, w,
      (i / 120) % 3 ? unsigned (i / 120 + 1) : 0, true, .2f, m, 650 + float (i % 900), float (i % 8) * 5, (i % 200) > 20, true);
    std::fwrite (&r.angles.x, 4, 1, stdout);
    std::fwrite (&r.angles.y, 4, 1, stdout);
    const unsigned char fire = r.fire ? 1 : 0;
    std::fwrite (&fire, 1, 1, stdout);
  }
}
