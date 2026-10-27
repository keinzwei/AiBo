// Bounded, local procedural policies for Human Copycat. GPL-2.0-or-later.
#include "copycat_lite.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rbcc {
uint32_t LiteRng::next(){state_^=state_<<13;state_^=state_>>17;state_^=state_<<5;return state_;}
float LiteRng::unit(){return float(next()>>8)*(1.f/16777216.f);}
unsigned LiteRng::bounded(unsigned n){return n?unsigned(uint64_t(next())*n>>32):0;}
int weightedChoice(const float* w,size_t n,LiteRng& rng){
    float total=0;for(size_t i=0;i<n;++i)if(std::isfinite(w[i])&&w[i]>0)total+=w[i];
    if(!(total>0)||!std::isfinite(total))return -1;
    float choice=rng.unit()*total;
    for(size_t i=0;i<n;++i)if(std::isfinite(w[i])&&w[i]>0){choice-=w[i];if(choice<0)return int(i);}
    for(size_t i=n;i>0;--i)if(std::isfinite(w[i-1])&&w[i-1]>0)return int(i-1);
    return -1;
}
bool preferredGun(unsigned w){return w==18||w==28||w==22||w==26||w==15||w==19||w==16;}
int preferredPrimary(int team,int money,int current,unsigned snipers,unsigned teammates,float taste,LiteRng& rng){
    if(team!=1&&team!=2)return 0;
    // Keep a surviving good gun, including a legally picked-up enemy rifle.
    if(current==18||current==28||current==22||current==15)return 0;
    if(current==19&&(money<4300||rng.unit()<.5f))return 0;
    const int ids[]={18,28,22,15,19},cost[]={4950,2750,3350,2450,1700};
    float w[]={10+24*std::max(0.f,std::min(1.f,taste)),62,62,26,18};
    if(team==1){w[2]=0;w[3]=0;}else w[1]=0;
    if(snipers>=std::max(1u,(teammates+2)/3))w[0]=0;
    for(unsigned i=0;i<5;++i)if(money<cost[i]||current==ids[i])w[i]=0;
    int pick=weightedChoice(w,5,rng);return pick<0?0:ids[pick];
}
int preferredSecondary(int money,int current,bool primary,LiteRng& rng){
    if(current==26)return 0;
    if(current==16&&(money<2200||rng.unit()<.65f))return 0;
    if(primary&&money<1800)return 0;
    if(money>=750)return rng.unit()<.72f?26:(current==16?0:16);
    return money>=600&&current!=16?16:0;
}
GunTuning gunTuning(unsigned w,float d){
    const bool far=d>850,near=d>0&&d<300;
    GunTuning g={240,180,220,4,false,false};
    switch(w){
    case 18:g={20,1250,10,std::max(.12f,std::min(.65f,std::atan2(7.f,std::max(200.f,d))*57.2957795f)),true,true};break;
    case 26:g={20,far?270.f:190.f,far?22.f:55.f,far?1.2f:2.f,true,false};break;
    case 16:g={20,far?190.f:135.f,far?40.f:100.f,far?1.7f:3.f,true,false};break;
    case 28:g={near?600.f:(far?20.f:190.f),far?230.f:110.f,near?110.f:35.f,far?.8f:2.6f,far,false};break;
    case 22:g={near?600.f:(far?20.f:185.f),far?210.f:100.f,near?120.f:40.f,far?.85f:2.8f,far,false};break;
    case 15:g={near?550.f:(far?20.f:180.f),far?215.f:110.f,near?110.f:38.f,far?.9f:2.8f,far,false};break;
    case 19:g={near?500.f:(far?170.f:320.f),far?140.f:85.f,far?100.f:240.f,far?2.4f:4.f,false,false};break;
    default:break;
    }
    return g;
}
void composeName(char* out,size_t n,unsigned identity,LiteRng& rng){
    if(!out||!n)return;
    static const char* const handles[]={"NEO","f0rest","GeT_RiGhT","markeloff","Edward","TaZ","cArn","walle",
        "HeatoN","SpawN","Potti","elemeNt","trace","Gux","kuben","loord","dsn","ave","zonic","Sunde",
        "ceh9","starix","Zeus","KAPiO","gob_b","roman","REAL","face","RobbaN","zet","Xizt","Friis"};
    static const char* const suffix[]={"","","",".cfg","_cs",".one","-mix","_1337"};
    const char* base=handles[identity%32];unsigned style=rng.bounded(10);
    if(style<8)std::snprintf(out,n,"%s%s",base,suffix[style]);
    else std::snprintf(out,n,"%s_%02u",base,rng.bounded(100));
}
void composeChat(char* out,size_t n,LiteChatEvent event,LiteRng& rng){
    if(!out||!n)return;
    static const char* const start[]={"","","okay, ","alright, ","well, ","come on, "};
    static const char* const end[]={"",""," for now"," this time"," then","..."};
    static const char* const round[]={"try another route","take it slow","keep some space","check the corners","watch the timing","stick together","look for an opening","change the pace"};
    static const char* const reply[]={"ready","agreed","sounds good","I will try","watch the situation","keep going","let us try again","stay calm"};
    static const char* const death[]={"nice shot","bad timing on my part","I rushed that","caught out of position","time for another approach","walked into that one","need to watch that angle","next round"};
    static const char* const utility[]={"blocking that sightline","making room to push","spread out a little","give it a moment","check those positions","clearing the way","keep your distance","watch the flash"};
    static const char* const hold[]={"hold this angle","wait for the timing","watch the corner","listen for footsteps","check before pushing","look for a chance","give it a second","stay patient"};
    const char* const* parts=event==ChatRound?round:(event==ChatDeath?death:(event==ChatUtility?utility:(event==ChatHold?hold:reply)));
    std::snprintf(out,n,"%s%s%s",start[rng.bounded(6)],parts[rng.bounded(8)],end[rng.bounded(6)]);
}
unsigned desiredBots(unsigned quota,unsigned clients,unsigned others){
    quota=std::min(31u,quota);clients=std::min(32u,clients);
    // Keep one joinable slot even on an empty dedicated server.
    unsigned used=std::max(1u,others);return used>=clients?0:std::min(quota,clients-used);
}
} // namespace rbcc
