// Behavioral invariants for local policies, not a claim about real CS accuracy/FPS.
#include "../src/copycat/copycat_lite.h"
#include "../src/copycat/copycat_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <set>
#include <string>
#include <limits>
using namespace rbcc;
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();}}while(0)
static void economy(){
    LiteRng rng(13429);unsigned counts[31]={0};
    for(int team=1;team<=2;++team)for(int money=0;money<=16000;money+=50){
        int w=preferredPrimary(team,money,-1,0,5,.5f,rng);CHECK(w==0||w==18||w==28||w==22||w==15||w==19);
        CHECK(team==1?(w!=22&&w!=15):w!=28);
        int price=w==18?4950:(w==28?2750:(w==22?3350:(w==15?2450:(w==19?1700:0))));CHECK(price<=money);
        CHECK(preferredPrimary(team,money,18,0,5,.5f,rng)==0);
        CHECK(preferredPrimary(team,money,28,0,5,.5f,rng)==0);
        CHECK(preferredPrimary(team,money,22,0,5,.5f,rng)==0);
    }
    for(unsigned i=0;i<4000;++i){int w=preferredPrimary(i%2+1,16000,-1,0,5,.8f,rng);++counts[w];
        CHECK(preferredPrimary(1,16000,-1,2,5,.9f,rng)!=18);
        CHECK(preferredPrimary(2,16000,-1,1,3,.9f,rng)!=18);
    }
    CHECK(counts[18]>100&&counts[28]>100&&counts[22]>100&&counts[15]>100&&counts[19]>100);
    for(unsigned i=0;i<1000;++i){int w=preferredSecondary(800,17,false,rng);CHECK(w==16||w==26);CHECK(preferredSecondary(16000,26,true,rng)==0);}
    CHECK(preferredSecondary(0,17,false,rng)==0);CHECK(preferredPrimary(0,16000,-1,0,5,1,rng)==0);
}
static void textAndQuota(){
    LiteRng rng(98765);std::set<std::string> names;
    for(unsigned i=0;i<4096;++i){char name[33];composeName(name,sizeof(name),i,rng);names.insert(name);CHECK(std::strlen(name)<33);
        CHECK(!std::strchr(name,'"')&&!std::strchr(name,';')&&!std::strchr(name,'\n'));}
    CHECK(names.size()>500);
    for(int event=ChatRound;event<=ChatHold;++event){std::set<std::string> sentences;
        for(unsigned i=0;i<600;++i){char text[128];composeChat(text,sizeof(text),LiteChatEvent(event),rng);sentences.insert(text);
            CHECK(std::strlen(text)>0&&std::strlen(text)<128);CHECK(!std::strchr(text,'"')&&!std::strchr(text,'\n'));
            std::string words=" "+std::string(text)+" ";const char* excluded[]={" aku "," kamu "," ayo "," siap "," coba "," musuh "," jangan "," mantap "," dulu "," jalan "};
            for(unsigned k=0;k<sizeof(excluded)/sizeof(excluded[0]);++k)CHECK(words.find(excluded[k])==std::string::npos);}
        CHECK(sentences.size()>80);
    }
    for(unsigned maxClients=0;maxClients<=32;++maxClients)for(unsigned humans=0;humans<=32;++humans){
        unsigned count=desiredBots(99,maxClients,humans);CHECK(count<=31);CHECK(count+humans<=maxClients||count==0);}
    CHECK(desiredBots(9,10,1)==9);CHECK(desiredBots(9,8,1)==7);CHECK(desiredBots(9,32,0)==9);CHECK(desiredBots(0,32,1)==0);
    CHECK(desiredBots(31,32,1)==31);CHECK(desiredBots(31,32,2)==30);CHECK(desiredBots(99,32,0)==31);
    float empty[]={0,-1,std::numeric_limits<float>::quiet_NaN()};CHECK(weightedChoice(empty,3,rng)==-1);
    unsigned selected[4]={0};float weights[]={30,30,25,15};for(unsigned i=0;i<10000;++i){int k=weightedChoice(weights,4,rng);CHECK(k>=0&&k<4);++selected[k];}
    for(unsigned k=0;k<4;++k)CHECK(selected[k]>900&&selected[k]<4000);
}
static unsigned fireCount(uint8_t w,float speed,bool scoped,float distance){
    HumanAim aim;SkillModel m;m.aimError=.15f;aim.reset(Vec(),43);unsigned shots=0;bool wasFire=false;
    for(unsigned tick=0;tick<500;++tick){AimOutput out=aim.update(.01f,Vec(),Vec(),w,99,true,0,m,distance,speed,scoped,true);
        if(out.fire){++shots;if(w==16||w==26||w==18)CHECK(!wasFire);}wasFire=out.fire;}
    return shots;
}
static void firearms(){
    CHECK(fireCount(18,100,true,1000)==0);CHECK(fireCount(18,0,false,1000)==0);
    unsigned awp=fireCount(18,0,true,1000);CHECK(awp>=2&&awp<=5);
    unsigned deagle=fireCount(26,0,false,500),usp=fireCount(16,0,false,500);CHECK(deagle>=12&&deagle<=30);CHECK(usp>=deagle);
    CHECK(fireCount(28,180,false,1000)==0);CHECK(fireCount(22,180,false,1000)==0);CHECK(fireCount(15,180,false,1000)==0);
    CHECK(fireCount(19,180,false,250)>100);
    CHECK(fireCount(28,0,false,1300)>=15&&fireCount(28,0,false,1300)<=28);
    CHECK(fireCount(22,0,false,1300)>=15&&fireCount(22,0,false,1300)<=28);
    CHECK(fireCount(15,0,false,1300)>=15&&fireCount(15,0,false,1300)<=28);
    CHECK(gunTuning(28,1300).burstMs<gunTuning(28,200).burstMs);
    CHECK(gunTuning(22,1300).burstMs<gunTuning(22,200).burstMs);
}
int main(){economy();textAndQuota();firearms();std::printf("PASS %u lite-policy checks: legal/economic weapon choices, AWP limits, tap/scope/movement gates, procedural text diversity, quotas, weighted exploration.\n",checks);}
