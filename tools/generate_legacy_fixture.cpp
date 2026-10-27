// Compile this against the ORIGINAL attached RealBot 4.2.4 core, not the fork.
// GPL-2.0-or-later. A synthetic, non-personal RBCC0003 compatibility fixture.
#include "copycat_core.h"
#include <string>
#include <cstdio>
int main (int argc, char **argv) {
  if (argc != 2) return 2;
  rbcc::Database db; rbcc::Clip c; c.profile = 424;
  for (unsigned i=0;i<30;++i) {
    rbcc::Frame f; f.msec=10; f.weapon=28; f.state=rbcc::Ground;
    f.context.team=0; f.origin=rbcc::Vec(float(i)*.5f,0,0); f.forward=50;
    c.frames.push_back(f); c.durationMs+=10;
  }
  c.end=c.frames.back().origin; c.features=rbcc::analyze(c.frames);
  if(!db.add(c))return 3;
  std::string error; if(!db.save(argv[1],"de_dust2",0x1234abcd,error)) {std::fprintf(stderr,"%s\n",error.c_str());return 4;}
  return 0;
}
