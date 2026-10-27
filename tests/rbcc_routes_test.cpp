// GPL-2.0-or-later. Synthetic split and human-route invariants.
#include "../src/copycat/copycat_routes.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
using namespace rbcc;
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<"\n";return 1;}}while(0)
static Frame at(float x,float y=0){Frame f;f.origin=Vec(x,y,36);f.state=Ground;f.msec=100;f.weapon=28;f.context.team=0;f.velocity=Vec(200,0,0);f.forward=200;return f;}
int main(){
    unsigned changed=0;
    for(uint32_t seed=1;seed<=100;++seed){std::vector<SplitMember> people={{0,0,false},{1,0,false},{2,1,false},{3,1,false},{4,2,true}};
        auto roles=splitRoles(people,seed);int n[3]={0,0,0};for(auto role:roles){CHECK(role>=0&&role<3);++n[role];}
        CHECK(n[0]==2&&n[1]==2&&n[2]==1);CHECK(roles[4]!=2);
        bool different=false;for(size_t i=0;i<roles.size();++i)if(roles[i]!=people[i].previous)different=true;if(different)++changed;
    }
    CHECK(changed>80);CHECK(splitRoles({},1).empty());
    for(int count=1;count<=32;++count){std::vector<SplitMember> p;for(int i=0;i<count;++i)p.push_back({i,-1,false});auto roles=splitRoles(p,42);
        CHECK(roles.size()==size_t(count));if(count>=2){bool a=false,b=false;for(int r:roles){a=a||r==0;b=b||r==1;}CHECK(a&&b);}}
    HumanRoutes nav;
    for(int x=0;x<=640;x+=64)nav.observe(at(float(x)),0,false);
    auto accept=[](const Vec&,const Vec&,uint8_t){return true;};
    auto route=nav.plan(Vec(0,0,36),Vec(640,0,36),0,1,accept);
    CHECK(route.size()>=8);CHECK(nav.nodes()[route.back()].origin.x>500);CHECK(nav.plan(Vec(0,0,36),Vec(640,0,36),1,1,accept).empty());
    // A second recorded approach is independent geometry, not a variation of view jitter.
    nav.resetTeacher(0);nav.observe(at(0),0,false);nav.observe(at(0,64),0,false);
    for(int x=0;x<=640;x+=64)nav.observe(at(float(x),128),0,false);
    nav.observe(at(640,64),0,false);nav.observe(at(640),0,false);
    auto blocked=[](const Vec& a,const Vec& b,uint8_t){return !(a.y<20&&b.y<20&&a.x>100&&a.x<550);};
    auto alternative=nav.plan(Vec(0,0,36),Vec(640,0,36),0,7,blocked);CHECK(!alternative.empty());
    bool upper=false;for(auto n:alternative)upper=upper||nav.nodes()[n].origin.y>100;CHECK(upper);
    Frame still=at(640);still.forward=0;still.velocity=Vec();still.state=Ground|Crouched;still.angles=Vec(0,42,0);
    for(int i=0;i<45;++i)nav.observe(still,0,true);
    int hide=nav.favoriteHide(Vec(640,0,36),0,3);CHECK(hide>=0);CHECK(nav.nodes()[size_t(hide)].hides[0]>0);
    CHECK(nav.favoriteHide(Vec(640,0,36),1,3)<0);
    std::string error;CHECK(nav.save("routes_test.hnav","de_test",123,error));HumanRoutes loaded;
    CHECK(loaded.load("routes_test.hnav","de_test",123,error));CHECK(loaded.nodes().size()==nav.nodes().size());CHECK(loaded.edgeCount()==nav.edgeCount());
    CHECK(!loaded.load("routes_test.hnav","de_test",124,error));CHECK(!loaded.load("routes_test.hnav","de_other",123,error));
    CHECK(loaded.nodes().size()==nav.nodes().size());
    std::ifstream file("routes_test.hnav",std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});bytes[30]^=1;
    std::ofstream bad("routes_bad.hnav",std::ios::binary);bad.write(bytes.data(),std::streamsize(bytes.size()));bad.close();
    CHECK(!loaded.load("routes_bad.hnav","de_test",123,error));CHECK(loaded.nodes().size()==nav.nodes().size());
    auto count=nav.edgeCount();nav.observe(at(10000),0,false);CHECK(nav.edgeCount()==count); // no teleport edge
    CHECK(nav.plan(Vec(0,0,36),Vec(640,0,36),0,1,[](const Vec&,const Vec&,uint8_t){return false;}).empty());
    std::remove("routes_test.hnav");std::remove("routes_bad.hnav");
    std::cout<<"Human route/split checks: "<<checks<<" passed\n";return 0;
}
