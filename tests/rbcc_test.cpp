// Synthetic tests of software invariants, not claims of GoldSrc game behavior.
#include "../src/copycat/copycat_core.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <chrono>
using namespace rbcc;
static unsigned checks=0;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<"FAIL line "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
static Frame frame(int tick=0){Frame f;f.msec=10;f.weapon=28;f.state=Ground;f.context.team=0;f.origin=Vec(tick*.5f,0,0);f.velocity=Vec(50,0,0);f.forward=50;return f;}
static void refresh(Clip& c){c.durationMs=0;for(size_t i=0;i<c.frames.size();++i){c.durationMs+=c.frames[i].msec;c.frames[i].previousButtons=i?c.frames[i-1].buttons:0;}
    c.features=analyze(c.frames);for(size_t i=0;i<c.throws.size();++i)c.features|=c.throws[i].weapon==4?HE:(c.throws[i].weapon==9?Smoke:Flash);
    c.end=c.frames.back().origin+Vec(.5f,0,0);}
static Clip clip(unsigned count=300){Clip c;c.profile=123;for(unsigned i=0;i<count;++i)c.frames.push_back(frame(int(i)));refresh(c);return c;}
static Query query(const Clip& c){Query q;q.current=c.frames[0];q.weapons=0x7fffffff;q.hasGoal=true;q.goal=Vec(1024,0,0);return q;}
static void recording(){
    Database db;Recorder r;
    for(int i=0;i<310;++i)r.capture(frame(i),42,db);
    CHECK(db.size()==1);CHECK(db.clips()[0].profile==42);CHECK(db.clips()[0].durationMs==2500);CHECK(validClip(db.clips()[0]));
    CHECK(db.clips()[0].frames[20].forward==50);CHECK(db.clips()[0].frames[20].msec==10);
    r.reset();Frame bad=frame();bad.origin.x=std::numeric_limits<float>::quiet_NaN();CHECK(!validFrame(bad));r.capture(bad,42,db);CHECK(r.pendingFrames()==0);
    r.capture(frame(),42,db);Frame teleport=frame();teleport.origin.x=2000;r.capture(teleport,42,db);CHECK(r.pendingFrames()==1);
    r.reset();Frame held=frame();held.buttons=Duck;r.capture(held,42,db);CHECK(r.pendingFrames()==0);
    r.reset();for(int i=0;i<100;++i){Frame f=frame(i);if(i>0)f.buttons=Duck;r.capture(f,42,db);}r.reset();CHECK(db.size()==1);
    // Team and objective changes cannot bridge a demonstration.
    for(int i=0;i<120;++i){Frame f=frame(i);if(i>=60)f.context.team=1;r.capture(f,42,db);}CHECK(r.pendingFrames()<=60);
}
static void highRate(){
    Recorder r;Database db;
    for(int i=0;i<2700;++i){Frame f=frame();f.msec=1;f.origin.x=i*.05f;if(i==100||i==120||i==145)f.buttons=Duck;r.capture(f,77,db);}
    CHECK(db.size()==1);CHECK(db.clips()[0].frames.size()==2500);CHECK(db.clips()[0].features&DoubleDuck);
    Playback p;CHECK(p.start(db.clips()[0]));unsigned pulses=0,total=0;
    while(!p.finished()){CHECK(p.advance(17));while(const Frame* f=p.due()){if(f->buttons&Duck)++pulses;p.consume();++total;}}
    CHECK(pulses==3);CHECK(total==2500);
}
static void techniques(){
    Clip c=clip();c.frames[30].buttons=Duck;c.frames[35].buttons=Duck;c.frames[45].buttons=Duck;
    c.frames[70].buttons=Jump;c.frames[70].side=250;refresh(c);CHECK(validClip(c));
    CHECK(c.features&DoubleDuck);CHECK(c.features&DuckCadence);CHECK(c.features&StrafeJump);
    Playback p;CHECK(p.start(c));unsigned ticks=0,ducks=0,jumps=0;std::vector<uint16_t> sequence;
    while(!p.finished()){CHECK(p.advance(17));while(const Frame* f=p.due()){sequence.push_back(f->buttons);if(f->buttons&Duck)++ducks;if(f->buttons&Jump)++jumps;p.consume();++ticks;}}
    CHECK(ticks==300);CHECK(ducks==3);CHECK(jumps==1);for(size_t i=0;i<300;++i)CHECK(sequence[i]==c.frames[i].buttons);
    CHECK(p.start(c));CHECK(p.advance(9));CHECK(!p.due());CHECK(p.advance(1));CHECK(p.due());p.consume();CHECK(p.cursor()==1);
    CHECK(!p.advance(101));CHECK(!p.active());CHECK(!p.advance(std::numeric_limits<double>::quiet_NaN()));
    c.frames.back().buttons=Jump;refresh(c);CHECK(!validClip(c));
}
static Clip grenade(uint8_t w){Clip c=clip();for(size_t i=0;i<c.frames.size();++i)c.frames[i].weapon=w;
    for(size_t i=50;i<100;++i)c.frames[i].buttons=Attack;
    Throw t;t.frame=100;t.weapon=w;t.observed=1;t.landing=Vec(1000,250,0);c.throws.push_back(t);refresh(c);return c;}
static void grenades(){
    for(unsigned k=0;k<3;++k){uint8_t w=k==0?4:(k==1?9:25);Clip c=grenade(w);CHECK(validClip(c));
        Database db;CHECK(db.add(c)!=0);Query q=query(c);CHECK(db.match(q));q.utility=false;CHECK(!db.match(q));q.utility=true;q.weapons&=~(1u<<w);CHECK(!db.match(q));
        c.throws[0].observed=0;CHECK(!validClip(c));}
    Database db;Recorder r;
    for(int i=0;i<330;++i){Frame f=frame(i);f.weapon=9;if(i>=50&&i<100)f.buttons=Attack;r.capture(f,10,db);
        if(i==260)r.noteLanding(9,Vec(900,0,0));}
    CHECK(db.size()==1);CHECK(db.clips()[0].features&Smoke);CHECK(db.clips()[0].throws[0].observed==1);
    // An unobserved or truncated throw is never admitted.
    Database incomplete;Recorder r2;for(int i=0;i<950;++i){Frame f=frame(i);f.weapon=4;if(i>=50&&i<100)f.buttons=Attack;r2.capture(f,11,incomplete);}CHECK(incomplete.size()==0);
}
static void matching(){
    Database db;Clip c=clip();uint32_t id=db.add(c);CHECK(id);Query q=query(c);size_t inspected=0;CHECK(db.match(q,&inspected));CHECK(inspected==1);
    q.current.context.team=1;CHECK(!db.match(q));q=query(c);q.current.context.objective=1;CHECK(!db.match(q));
    q=query(c);q.current.origin.x=200;CHECK(!db.match(q));q=query(c);q.current.velocity.x=400;CHECK(!db.match(q));
    q=query(c);q.profile=999;CHECK(!db.match(q));q.profile=123;CHECK(db.match(q));q.excludeId=id;CHECK(!db.match(q));
    q=query(c);q.current.context.visible=1;CHECK(!db.match(q));q=query(c);q.goal=Vec(-1000,0,0);CHECK(!db.match(q));
    q=query(c);q.current.angles.y=180;CHECK(!db.match(q));q=query(c);q.weapons=1u<<29;q.current.weapon=29;CHECK(db.match(q));q.current.maxSpeed=200;CHECK(!db.match(q));
    db.feedback(id,false);db.feedback(id,true);CHECK(db.clips()[0].failures==1);CHECK(db.clips()[0].successes==1);
    for(unsigned i=0;i<350;++i)CHECK(db.add(c));q=query(c);q.candidates=32;CHECK(db.match(q,&inspected));CHECK(inspected<=32);
}
static std::vector<unsigned char> read(const char* p){std::ifstream f(p,std::ios::binary);return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());}
static void write(const char* p,const std::vector<unsigned char>& bytes){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
static void persistence(){
    Database db;CHECK(db.add(clip()));CHECK(db.add(grenade(25)));std::string error;
    CHECK(db.save("build/tests/data.rbcc","de_dust2",456,error));Database loaded;CHECK(loaded.load("build/tests/data.rbcc","de_dust2",456,error));
    CHECK(loaded.size()==2);CHECK(loaded.frameCount()==600);CHECK(loaded.clips()[1].throws[0].landing.y==250);
    CHECK(!loaded.load("build/tests/data.rbcc","de_nuke",456,error));CHECK(loaded.size()==2);
    CHECK(!loaded.load("build/tests/data.rbcc","de_dust2",457,error));CHECK(loaded.size()==2);
    std::vector<unsigned char> original=read("build/tests/data.rbcc"),bytes=original;bytes[50]^=128;write("build/tests/bad.rbcc",bytes);
    CHECK(!loaded.load("build/tests/bad.rbcc","de_dust2",456,error));CHECK(loaded.size()==2);
    for(size_t size=0;size<120;++size){bytes.assign(original.begin(),original.begin()+size);write("build/tests/bad.rbcc",bytes);CHECK(!loaded.load("build/tests/bad.rbcc","de_dust2",456,error));CHECK(loaded.size()==2);}
    // Bounded mutation fuzzing with a VALID checksum exercises the parser, not only its checksum gate.
    uint32_t rng=12345;for(unsigned i=0;i<250;++i){bytes=original;rng=rng*1664525u+1013904223u;size_t at=20+rng%(bytes.size()-24);bytes[at]^=static_cast<unsigned char>((rng>>16)|1);
        uint32_t h=hashBytes(bytes.data(),bytes.size()-4);for(unsigned j=0;j<4;++j)bytes[bytes.size()-4+j]=static_cast<unsigned char>(h>>(8*j));
        write("build/tests/bad.rbcc",bytes);Database trial;trial.load("build/tests/bad.rbcc","de_dust2",456,error);CHECK(trial.size()<=MaxClips);}
    CHECK(db.save("build/tests/data.rbcc","de_dust2",456,error));CHECK(!db.save("build/tests/missing/data.rbcc","de_dust2",456,error));
}
static void aimAndStress(){
    HumanAim a;SkillModel defaults;a.reset(Vec(0,179,0),19);AimOutput out=a.update(.01f,Vec(0,-179,0),Vec(),28,1,true,0,defaults);
    CHECK(!out.fire);CHECK(std::fabs(angleDelta(out.angles.y,179))<1); // wrapped target, no snap
    a.reset(Vec(0,0,0),19);float old=0;unsigned shots=0,pauses=0;
    for(unsigned i=0;i<400;++i){out=a.update(.01f,Vec(0,90,0),Vec(-2,0,0),28,2,true,.3f,defaults);
        CHECK(std::fabs(angleDelta(out.angles.y,old))<=defaults.turnRate*.01f+.001f);old=out.angles.y;if(i<10)CHECK(!out.fire);if(out.fire)++shots;else ++pauses;}
    CHECK(shots>0);CHECK(pauses>0);CHECK(out.angles.x>-1&&out.angles.x<4); // imperfect recoil correction
    out=a.update(.01f,Vec(0,90,0),Vec(),28,3,true,0,defaults);CHECK(!out.fire); // target switch reaction
    for(unsigned i=0;i<100;++i){out=a.update(.01f,Vec(),Vec(),28,0,true,0,defaults);CHECK(!out.fire);}
    HumanStress s;CHECK(!s.wantsCover(defaults,100));s.update(.02f,20,true,0,false);CHECK(s.wantsCover(defaults,20));
    float stressed=s.value();for(unsigned i=0;i<600;++i)s.update(.02f,20,false,3,false);CHECK(s.value()<stressed*.1f);
    Clip c=clip();for(unsigned i=20;i<300;++i){Frame& f=c.frames[i];f.context.visible=1;f.context.targetToken=123;f.context.enemy=Vec(1000,0,0);f.context.enemyDistance=1000;f.context.enemyYaw=.8f;f.context.enemyPitch=.4f;f.health=35;
        if(i>=45&&i<70)f.buttons=Attack;}
    refresh(c);SkillModel learned;learned.learn(c);CHECK(learned.reactions>0);CHECK(learned.bursts>0);CHECK(learned.aims>0);CHECK(learned.reactionMs>=100&&learned.reactionMs<=800);
}
static void spatialBudget(){
    Database db;Clip c=clip(128);
    for(unsigned i=0;i<1024;++i){Clip moved=c;float y=float(i%16)*64;for(size_t j=0;j<moved.frames.size();++j)moved.frames[j].origin.y+=y;moved.end.y+=y;CHECK(db.add(moved));}
    for(unsigned i=0;i<16;++i){Query q=query(c);q.current.origin.y=float(i)*64;size_t count=0;CHECK(db.match(q,&count));CHECK(count<=128);}
}
static void capacity(){
    Database db;Clip c=clip(150);for(unsigned i=0;i<1100;++i){c.frames[0].origin=Vec();CHECK(db.add(c));}CHECK(db.size()<=MaxClips);CHECK(db.frameCount()<=MaxFrames);
    Query q=query(c);size_t checked=0;CHECK(db.match(q,&checked));CHECK(checked<=128);
    CHECK(db.clips().front().id>1);
    // Rare utility must survive a long session of ordinary movement recordings.
    Database diverse;CHECK(diverse.add(grenade(9)));for(unsigned i=0;i<1100;++i)CHECK(diverse.add(c));
    CHECK(diverse.clips().front().features&Smoke);
}
int main(){recording();highRate();techniques();grenades();matching();persistence();aimAndStress();spatialBudget();capacity();std::cout<<"PASS "<<checks<<" checks; capture, command timing, duck/jump, utility outcomes, context matching, bounded storage, parser fuzz, human aim, recoil, stress.\n";}
