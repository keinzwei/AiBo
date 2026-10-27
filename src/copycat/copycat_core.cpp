// Human Copycat extension for RealBot. GPL-2.0-or-later; see COPYING.
#include "copycat_core.h"
#include "copycat_lite.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace rbcc {
static float clamp(float v,float lo,float hi) { return std::max(lo,std::min(hi,v)); }
static bool finite(float f) { return std::isfinite(f); }
static bool finiteVec(const Vec& v) { return finite(v.x)&&finite(v.y)&&finite(v.z); }
float Vec::length() const { return std::sqrt(x*x+y*y+z*z); }
float distance(const Vec& a,const Vec& b) { return (a-b).length(); }
float angleDelta(float a,float b) { float d=std::fmod(a-b,360.f); if(d>180)d-=360; if(d<-180)d+=360; return d; }
bool isUtility(unsigned w) { return w==4||w==9||w==25; }
uint32_t hashBytes(const void* bytes,size_t n,uint32_t h) {
    const unsigned char* p=static_cast<const unsigned char*>(bytes);
    for(size_t i=0;i<n;++i) { h^=p[i]; h*=16777619u; } return h;
}
bool Cell::operator<(const Cell& b) const { return x!=b.x?x<b.x:(y!=b.y?y<b.y:z<b.z); }
static Cell cell(const Vec& p) { Cell c={int(std::floor(p.x/128)),int(std::floor(p.y/128)),int(std::floor(p.z/128))}; return c; }
static bool neutral(const Frame& f) {
    return (f.state&Ground) && !(f.state&(Ladder|Water)) &&
        !(f.buttons&(Attack|Attack2|Jump|Duck|Use)) && !(f.previousButtons&(Attack|Attack2|Jump|Duck|Use));
}
bool validFrame(const Frame& f) {
    return f.msec>=1 && f.msec<=100 && f.weapon>=1 && f.weapon<=30 &&
        finiteVec(f.origin)&&finiteVec(f.velocity)&&finiteVec(f.angles)&&finiteVec(f.punch)&&
        f.origin.length()<65536 && f.velocity.length()<5000 && f.punch.length()<180 &&
        finite(f.forward)&&finite(f.side)&&finite(f.up)&&finite(f.maxSpeed)&&f.maxSpeed>0&&f.maxSpeed<=2000&&
        std::fabs(f.forward)<=1000&&std::fabs(f.side)<=1000&&std::fabs(f.up)<=1000&&
        std::fabs(f.angles.x)<=360&&std::fabs(f.angles.y)<=360&&std::fabs(f.angles.z)<=360&&
        finiteVec(f.context.enemy)&&finite(f.context.enemyDistance)&&finite(f.context.enemyYaw)&&finite(f.context.enemyPitch)&&
        std::fabs(f.context.enemyYaw)<=360&&std::fabs(f.context.enemyPitch)<=360&&
        f.context.enemyDistance>=0&&f.context.enemyDistance<65536&&f.context.team<=1&&
        f.context.phase<=3&&f.context.objective<=15&&f.context.friends<=32&&f.context.visible<=1&&f.state<=15;
}
uint32_t analyze(const std::vector<Frame>& fs) {
    uint32_t flags=Navigation; unsigned ducks=0,ms=0,lastDuck=0; bool hadDuck=false;
    unsigned previous=fs.empty()?0:fs[0].previousButtons;
    for(size_t i=0;i<fs.size();++i) {
        const Frame& f=fs[i];
        if(f.buttons&Jump) { flags|=Jumping; if(std::fabs(f.side)>50) flags|=StrafeJump; }
        if((f.buttons&Duck)&&!(previous&Duck)) {
            if(hadDuck && ms-lastDuck<=500) flags|=DoubleDuck;
            ++ducks; lastDuck=ms; hadDuck=true;
        }
        if(f.buttons&Reload) flags|=Reloading;
        if((f.buttons&(Attack|Attack2))&&!isUtility(f.weapon)&&f.weapon!=6) flags|=Combat;
        if((f.buttons&Use)||f.weapon==6)flags|=Interaction;
        previous=f.buttons; ms+=f.msec;
    }
    if(ducks>=3) flags|=DuckCadence;
    if(fs.size()>1) {
        Vec displacement=fs.back().origin-fs.front().origin;
        if(displacement.length()<40) flags|=Holding;
        const Frame& f=fs.front();
        if(f.context.visible) {
            Vec toEnemy=f.context.enemy-f.origin;
            if(displacement.x*toEnemy.x+displacement.y*toEnemy.y < -1000) flags|=Retreat;
        }
        bool visible=false,lost=false;
        for(size_t i=0;i<fs.size();++i) {
            if(fs[i].context.visible) visible=true;
            if(visible&&!fs[i].context.visible) lost=true;
        }
        if(visible&&lost&&displacement.length()<160) flags|=Peeking;
    }
    return flags;
}
bool validClip(const Clip& c) {
    if(c.frames.size()<8||c.frames.size()>MaxClipFrames||c.throws.size()>MaxThrows||!finiteVec(c.end)||c.end.length()>=65536)return false;
    unsigned ms=0;
    for(size_t i=0;i<c.frames.size();++i) {
        if(!validFrame(c.frames[i])||c.frames[i].context.team!=c.frames[0].context.team ||
           c.frames[i].context.objective!=c.frames[0].context.objective) return false;
        if(i&&(distance(c.frames[i].origin,c.frames[i-1].origin)>192||c.frames[i].previousButtons!=c.frames[i-1].buttons)) return false;
        ms+=c.frames[i].msec;
    }
    if(ms<200||ms>MaxClipMs||c.durationMs!=ms||!neutral(c.frames.front())||distance(c.end,c.frames.back().origin)>192)return false;
    if(c.features!= (analyze(c.frames)|([&](){uint32_t f=0;for(size_t i=0;i<c.throws.size();++i)f|=c.throws[i].weapon==4?HE:(c.throws[i].weapon==9?Smoke:Flash);return f;})()))return false;
    for(size_t i=0;i<c.throws.size();++i) {
        const Throw& t=c.throws[i];
        if(t.frame>=c.frames.size()||!isUtility(t.weapon)||t.observed!=1||!finiteVec(t.landing)||t.landing.length()>65536)return false;
        const Frame& f=c.frames[t.frame];
        if(f.buttons&Attack||!(f.previousButtons&Attack))return false;
    }
    // Never save an armed grenade, a truncated jump, or a truncated duck cadence.
    const Frame& last=c.frames.back();
    if(last.buttons&(Attack|Attack2|Jump|Duck|Use)||!(last.state&Ground))return false;
    // Every grenade attack must have a release and an observed projectile outcome.
    bool armed=false; unsigned releases=0;
    for(size_t i=0;i<c.frames.size();++i) {
        const Frame& f=c.frames[i];
        if(isUtility(f.weapon)&&(f.buttons&Attack))armed=true;
        if(armed&&!(f.buttons&Attack)) { ++releases; armed=false; }
    }
    return !armed&&releases==c.throws.size();
}

static void mean(float& v,uint32_t& n,float sample) {
    if(n<1000000)++n;
    v+=(sample-v)/float(std::min(n,uint32_t(500))); // bounded-memory adaptation
}
SkillModel::SkillModel():reactionMs(240),turnRate(180),aimError(.65f),burstMs(240),pauseMs(180),coverHealth(45),
    aimHeight(0),heightSamples(0),
    reactions(0),turns(0),aims(0),bursts(0),pauses(0),retreats(0) {
    for(unsigned w=0;w<31;++w) { recoil[w]=.45f; recoilSamples[w]=0; }
}
void SkillModel::learn(const Clip& c) {
    unsigned reaction=0,burst=0,pause=0; bool awaiting=false;
    for(size_t i=1;i<c.frames.size();++i) {
        const Frame& p=c.frames[i-1]; const Frame& f=c.frames[i];
        float dt=f.msec*.001f;
        bool gun=!isUtility(f.weapon)&&f.weapon!=6&&f.weapon!=29;
        bool firing=gun&&(f.buttons&Attack);
        if(f.context.visible&&(!p.context.visible||p.context.targetToken!=f.context.targetToken)) { reaction=0; awaiting=true; }
        if(awaiting) {
            reaction+=f.msec;
            if(!f.context.visible||reaction>1200)awaiting=false;
            else if(firing) { if(reaction>=80)mean(reactionMs,reactions,clamp(float(reaction),100,800)); awaiting=false; }
        }
        float speed=std::sqrt(std::pow(angleDelta(f.angles.y,p.angles.y),2.f)+std::pow(angleDelta(f.angles.x,p.angles.x),2.f))/dt;
        if(f.context.visible&&speed>30&&speed<900)mean(turnRate,turns,clamp(speed*1.5f,60,540));
        if(firing&&f.context.visible) {
            // Derived from RBCC0003 observations, so the wire format stays intact.
            if(f.context.enemyDistance>80&&f.context.enemyDistance<1800&&std::fabs(f.context.enemyPitch)<8) {
                float height=std::tan(f.context.enemyPitch*.01745329252f)*f.context.enemyDistance;
                mean(aimHeight,heightSamples,clamp(height,-18,18));
            }
            float error=std::sqrt(f.context.enemyYaw*f.context.enemyYaw+f.context.enemyPitch*f.context.enemyPitch);
            if(error<12)mean(aimError,aims,clamp(error,.2f,4));
            if(p.weapon==f.weapon&&(p.buttons&Attack)&&p.context.targetToken==f.context.targetToken) {
                float recoilChange=f.punch.x-p.punch.x;
                float aimChange=f.context.enemyPitch-p.context.enemyPitch;
                if(std::fabs(recoilChange)>.05f&&std::fabs(recoilChange)<3&&std::fabs(aimChange)<8) {
                    // enemyPitch is target minus command angle: an increasing error during kick
                    // is evidence of downward human mouse compensation.
                    float k=clamp(aimChange/recoilChange,0,1.4f);
                    mean(recoil[f.weapon],recoilSamples[f.weapon],k);
                }
            }
        }
        if(firing) {
            if(!(p.buttons&Attack)&&pause>=60&&pause<1500)mean(pauseMs,pauses,clamp(float(pause),80,600));
            burst+=f.msec; pause=0;
        } else {
            if(burst>=50&&burst<1500)mean(burstMs,bursts,clamp(float(burst),80,600));
            burst=0; pause+=f.msec;
        }
        if(f.context.visible) {
            Vec movement=f.origin-p.origin, threat=f.context.enemy-f.origin;
            if(movement.x*threat.x+movement.y*threat.y < -30 && f.health<80)
                mean(coverHealth,retreats,clamp(float(f.health),20,75));
        }
    }
}
void Database::clear() { clips_.clear();index_.clear();models_.clear();frames_=0;nextId_=1; }
void Database::rebuild() {
    index_.clear();frames_=0;
    for(size_t i=0;i<clips_.size();++i) { frames_+=clips_[i].frames.size();index_[cell(clips_[i].frames[0].origin)].push_back(i); }
}
uint32_t Database::add(Clip c) {
    if(!validClip(c))return 0;
    while(!clips_.empty()&&(clips_.size()>=MaxClips||frames_+c.frames.size()>MaxFrames)) {
        size_t victim=0;float weakest=1e9f;
        for(size_t i=0;i<clips_.size();++i){const Clip& old=clips_[i];
            float priority=(old.features&Utility?120.f:0)+(old.features&(DoubleDuck|StrafeJump)?70.f:0)+
                (old.features&(Combat|Retreat|Peeking)?35.f:0)+std::min(20.f,float(old.successes)*2)-std::min(30.f,float(old.failures)*3);
            if(priority<weakest){weakest=priority;victim=i;}
        }
        frames_-=clips_[victim].frames.size();clips_.erase(clips_.begin()+victim);
    }
    c.requiredWeapons=0;for(size_t i=0;i<c.frames.size();++i)c.requiredWeapons|=1u<<c.frames[i].weapon;
    c.id=nextId_++;if(!nextId_)nextId_=1;
    if(models_.size()<65||models_.count(c.profile))models_[c.profile].learn(c);
    if(c.profile)models_[0].learn(c);
    clips_.push_back(c);rebuild();return c.id;
}
const SkillModel& Database::model(uint32_t p) const {
    std::map<uint32_t,SkillModel>::const_iterator it=models_.find(p);
    if(it!=models_.end())return it->second;
    it=models_.find(0);if(it!=models_.end())return it->second;
    static const SkillModel defaults;return defaults;
}
void Database::feedback(uint32_t id,bool ok) {
    for(size_t i=0;i<clips_.size();++i)if(clips_[i].id==id) {
        uint32_t& n=ok?clips_[i].successes:clips_[i].failures;if(n<1000000)++n;return;
    }
}
const Clip* Database::match(const Query& q,size_t* inspected) const {
    size_t checked=0,limit=std::max(size_t(16),std::min(size_t(256),q.candidates));
    const Clip* best=0;float score=1e9f;Cell center=cell(q.current.origin);
    for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)for(int dz=-1;dz<=1;++dz) {
        Cell key={center.x+dx,center.y+dy,center.z+dz};
        std::map<Cell,std::vector<size_t> >::const_iterator bucket=index_.find(key);
        if(bucket==index_.end())continue;
        // Newest demonstrations first, with a hard bound on work per decision.
        for(size_t k=bucket->second.size();k>0&&checked<limit;--k) {
            const Clip& c=clips_[bucket->second[k-1]];
            if(c.id==q.excludeId||(q.profile&&c.profile!=q.profile)||(c.failures>=5&&c.successes==0))continue;
            const Frame& f=c.frames[0]; bool util=(c.features&Utility)!=0;
            if((util&&!q.utility)||((c.features&Combat)&&!q.combat))continue;
            if(f.context.team!=q.current.context.team||f.context.objective!=q.current.context.objective||
               f.context.phase!=q.current.context.phase||f.context.visible!=q.current.context.visible)continue;
            if((f.state&(Ground|Crouched|Ladder|Water))!=(q.current.state&(Ground|Crouched|Ladder|Water)))continue;
            float dist=distance(f.origin,q.current.origin);
            if(dist>(util?16:40)||std::fabs(f.origin.z-q.current.origin.z)>12)continue;
            if(distance(f.velocity,q.current.velocity)>(util?55:100))continue;
            float yaw=std::fabs(angleDelta(f.angles.y,q.current.angles.y));
            if(yaw>(f.velocity.length()>35?4:(util?20:40))||std::fabs(angleDelta(f.angles.x,q.current.angles.x))>(f.velocity.length()>35?4:45))continue;
            if(f.context.visible&&(std::fabs(f.context.enemyYaw-q.current.context.enemyYaw)>20||
               std::fabs(f.context.enemyDistance-q.current.context.enemyDistance)>180))continue;
            bool weaponsOK=(q.weapons&c.requiredWeapons)==c.requiredWeapons;
            bool exactWeapon=(c.features&(Utility|Combat|Reloading|Interaction))!=0;
            if(exactWeapon&&(!weaponsOK||f.weapon!=q.current.weapon))continue;
            if(std::fabs(f.maxSpeed-q.current.maxSpeed)>12)continue;
            if(q.hasGoal&&(c.features&Holding)&&!f.context.visible&&distance(q.goal,q.current.origin)>128)continue;
            if(q.hasGoal&&!util&&!(c.features&(Retreat|Combat|Holding))) {
                Vec desired=q.goal-q.current.origin, travel=c.end-f.origin;
                if(desired.x*travel.x+desired.y*travel.y<0)continue;
            }
            ++checked;
            float s=dist+yaw*.4f+distance(f.velocity,q.current.velocity)*.1f+
                std::fabs(float(f.context.friends)-q.current.context.friends)*5+
                24*float(c.failures+1)/float(c.successes+c.failures+2);
            if(util)s-=(c.features&Flash)?32.f:((c.features&HE)?14.f:2.f);
            if(q.selectionSeed){uint32_t mixed=c.id^q.selectionSeed;mixed=hashBytes(&mixed,sizeof(mixed));s+=float(mixed&65535)*(7.f/65535.f);}
            if(s<score) {best=&c;score=s;}
        }
    }
    if(inspected)*inspected=checked;return best;
}
void Recorder::reset() { pending_=Clip();lastButtons_=0;sinceRelease_=quietMs_=0;valid_=false;haveLast_=false; }
void Recorder::start(uint32_t profile) { pending_=Clip();pending_.profile=profile;sinceRelease_=quietMs_=0;valid_=true; }
uint32_t Recorder::capture(const Frame& input,uint32_t profile,Database& db) {
    if(!validFrame(input)) {reset();return 0;}
    Frame f=input;f.previousButtons=lastButtons_;lastButtons_=f.buttons;
    if(haveLast_&&(distance(f.origin,last_.origin)>192||f.context.team!=last_.context.team||
       f.context.objective!=last_.context.objective||profile!=pending_.profile)) {valid_=false;pending_=Clip();}
    haveLast_=true;last_=f;uint32_t added=0;
    quietMs_=neutral(f)?quietMs_+f.msec:0;
    if(valid_&&(pending_.durationMs>=2500||pending_.frames.size()+256>=MaxClipFrames)&&neutral(f)&&quietMs_>=250) {
        bool outcomes=true;for(size_t i=0;i<pending_.throws.size();++i)if(!pending_.throws[i].observed)outcomes=false;
        if(outcomes&&(pending_.throws.empty()||sinceRelease_>=1000)) {
            pending_.end=f.origin;pending_.features=analyze(pending_.frames);
            for(size_t i=0;i<pending_.throws.size();++i)pending_.features|=pending_.throws[i].weapon==4?HE:(pending_.throws[i].weapon==9?Smoke:Flash);
            added=db.add(pending_);valid_=false;
        }
    }
    if(!valid_) {if(!neutral(f))return added;start(profile);}
    if(pending_.frames.size()>=MaxClipFrames||pending_.durationMs+f.msec>MaxClipMs) {
        valid_=false;pending_=Clip();return added;
    }
    if(!pending_.frames.empty()) {
        const Frame& p=pending_.frames.back();
        if(isUtility(p.weapon)&&(p.buttons&Attack)&&!(f.buttons&Attack)) {
            if(pending_.throws.size()>=MaxThrows){valid_=false;pending_=Clip();return added;}
            Throw t;t.frame=uint32_t(pending_.frames.size());t.weapon=p.weapon;pending_.throws.push_back(t);sinceRelease_=0;
        }
    }
    pending_.frames.push_back(f);pending_.durationMs+=f.msec;sinceRelease_+=f.msec;return added;
}
void Recorder::noteLanding(uint8_t weapon,const Vec& p) {
    if(!valid_||!finiteVec(p))return;
    for(size_t i=0;i<pending_.throws.size();++i)if(pending_.throws[i].weapon==weapon&&!pending_.throws[i].observed) {
        pending_.throws[i].landing=p;pending_.throws[i].observed=1;return;
    }
}
bool Playback::start(const Clip& c) { if(!validClip(c))return false;clip_=c;cursor_=0;credit_=0;active_=true;return true; }
bool Playback::advance(double ms) {
    if(!active_||!std::isfinite(ms)||ms<0||ms>100) {stop();return false;}
    credit_+=ms;if(credit_>150) {stop();return false;}return true;
}
const Frame* Playback::due() const {return active_&&cursor_<clip_.frames.size()&&credit_+.0001>=clip_.frames[cursor_].msec?&clip_.frames[cursor_]:0;}
void Playback::consume() {if(due()){credit_-=clip_.frames[cursor_].msec;++cursor_;}}
void Playback::stop() {active_=false;credit_=0;}

void HumanAim::reset(const Vec& a,uint32_t seed) {
    angles_=a;angularVelocity_=Vec();noise_=Vec();target_=0;reactionLeft_=burstLeft_=pauseLeft_=0;
    initialized_=true;shooting_=false;lastWeapon_=0;rng_=seed?seed:1;
}
float HumanAim::random() {rng_^=rng_<<13;rng_^=rng_>>17;rng_^=rng_<<5;return float(rng_&65535)/32767.5f-1;}
AimOutput HumanAim::update(float dt,const Vec& wanted,const Vec& punch,uint8_t weapon,
    uint32_t target,bool request,float stress,const SkillModel& m,float enemyDistance,float moveSpeed,bool scoped,bool weaponPolicy,float expertise,bool externalDiscipline) {
    if(!initialized_)reset(wanted,1);
    dt=clamp(dt,.001f,.1f);stress=clamp(stress,0,1);
    if(lastWeapon_!=weapon){lastWeapon_=weapon;shooting_=false;burstLeft_=0;pauseLeft_=0;}
    if(target!=target_) { target_=target;reactionLeft_=target?m.reactionMs*.001f*(1+.25f*stress)*(1+.15f*random()):0; }
    reactionLeft_=std::max(0.f,reactionLeft_-dt);pauseLeft_=std::max(0.f,pauseLeft_-dt);
    float persistence=std::exp(-dt/0.22f);
    noise_=noise_*persistence+Vec(random(),random(),0)*(std::sqrt(1-persistence*persistence)*m.aimError*(weapon==18?.20f:1.f)*(1+stress));
    Vec desired=wanted;
    if(target) {
        desired.x+=noise_.x;desired.y+=noise_.y;
        if(weapon<31) {float k=clamp(m.recoilSamples[weapon]?m.recoil[weapon]:.9f,0,1.4f);if(weapon==18)k=1;desired.x-=punch.x*k;desired.y-=punch.y*k*(.65f+.33f*clamp(expertise,0,1));}
    }
    if(target&&reactionLeft_>0)desired=angles_; // no instant acquisition or target switch
    float maxSpeed=clamp(m.turnRate,60,540)*(1-.22f*stress), acceleration=maxSpeed*5;
    float* axes[2]={&angles_.x,&angles_.y};float* speeds[2]={&angularVelocity_.x,&angularVelocity_.y};
    float desiredAxes[2]={desired.x,desired.y};
    for(unsigned i=0;i<2;++i) {
        float delta=angleDelta(desiredAxes[i],*axes[i]);float v=clamp(delta*8,-maxSpeed,maxSpeed);
        *speeds[i]+=clamp(v-*speeds[i],-acceleration*dt,acceleration*dt);
        float step=*speeds[i]*dt;if(std::fabs(step)>std::fabs(delta)&&step*delta>=0)step=delta;
        *axes[i]=angleDelta(*axes[i]+step,0);
    }
    angles_.x=clamp(angles_.x,-89,89);angles_.z=0;
    float aimError=std::sqrt(std::pow(angleDelta(desired.x,angles_.x),2.f)+std::pow(angleDelta(desired.y,angles_.y),2.f));
    GunTuning tuning=gunTuning(weapon,enemyDistance);bool tuned=weaponPolicy&&preferredGun(weapon);
    float maxError=tuned?tuning.maxError:6+stress*3;
    bool fire=request&&target&&reactionLeft_<=0&&pauseLeft_<=0&&aimError<maxError;
    if(tuned&&(moveSpeed>tuning.maxFireSpeed||(tuning.scope&&!scoped)))fire=false;
    if(tuned&&weapon==18){
        float physicalError=std::sqrt(std::pow(angleDelta(wanted.x,angles_.x+punch.x),2.f)+std::pow(angleDelta(wanted.y,angles_.y+punch.y),2.f));
        if(physicalError>maxError)fire=false;
    }
    if(externalDiscipline){AimOutput out={angles_,fire};return out;}
    float burst=clamp(m.burstMs,80,600),pause=clamp(m.pauseMs,80,600);
    if(tuned){burst=(enemyDistance>0&&enemyDistance<300&&weapon!=18&&weapon!=26&&weapon!=16)?std::max(burst,tuning.burstMs):std::min(burst,tuning.burstMs);pause=std::max(pause,tuning.pauseMs);}
    if(fire&&tuned&&tuning.tap){pauseLeft_=pause*.001f*(1+.12f*random());shooting_=false;AimOutput out={angles_,true};return out;}
    if(fire&&!shooting_)burstLeft_=burst*.001f*(1+.2f*random());
    if(fire) {burstLeft_-=dt;if(burstLeft_<=0){fire=false;pauseLeft_=pause*.001f*(1+.2f*random());}}
    shooting_=fire;AimOutput out={angles_,fire};return out;
}
float HumanStress::update(float dt,int health,bool enemy,unsigned friends,bool reload) {
    float damage=float(std::max(0,health_-health))/100;health_=health;
    float pressure=(enemy?.18f:0)+ (enemy&&friends==0?.20f:0)+(reload&&enemy?.18f:0)+
        (enemy?clamp((55-health)/100.f,0,.5f):0);
    value_=clamp(value_+damage*1.6f+(pressure-value_)*clamp(dt/1.5f,0,1),0,1);return value_;
}
bool HumanStress::wantsCover(const SkillModel& m,int health) const {return value_>.60f||(value_>.30f&&health<m.coverHealth);}

// Portable, bounded, checksummed format. No native struct layout or pointer data on disk.
struct Writer {
    std::vector<unsigned char> b;
    void u8(unsigned v){b.push_back(static_cast<unsigned char>(v));}
    void u32(uint32_t v){for(unsigned i=0;i<4;++i)u8(v>>(8*i));}
    void f32(float v){uint32_t u;std::memcpy(&u,&v,4);u32(u);}
    void vec(const Vec& v){f32(v.x);f32(v.y);f32(v.z);}
    void frame(const Frame& f){
        vec(f.origin);vec(f.velocity);vec(f.angles);vec(f.punch);f32(f.forward);f32(f.side);f32(f.up);f32(f.maxSpeed);
        u32(f.buttons);u32(f.previousButtons);u8(f.msec);u8(f.impulse);u8(f.weapon);u8(f.state);u8(f.health);
        u8(f.context.team);u8(f.context.phase);u8(f.context.objective);u8(f.context.friends);u8(f.context.visible);
        f32(f.context.enemyDistance);f32(f.context.enemyYaw);f32(f.context.enemyPitch);vec(f.context.enemy);u32(f.context.targetToken);
    }
};
struct Reader {
    const std::vector<unsigned char>& b;size_t p;bool ok;
    Reader(const std::vector<unsigned char>& bytes):b(bytes),p(0),ok(true){}
    uint8_t u8(){if(p>=b.size()){ok=false;return 0;}return b[p++];}
    uint32_t u32(){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(u8())<<(i*8);return v;}
    float f32(){uint32_t u=u32();float f;std::memcpy(&f,&u,4);return f;}
    Vec vec(){float x=f32(),y=f32(),z=f32();return Vec(x,y,z);}
    Frame frame(){Frame f;f.origin=vec();f.velocity=vec();f.angles=vec();f.punch=vec();f.forward=f32();f.side=f32();f.up=f32();f.maxSpeed=f32();
        uint32_t buttons=u32(),previous=u32();if(buttons>65535||previous>65535)ok=false;f.buttons=uint16_t(buttons);f.previousButtons=uint16_t(previous);
        f.msec=u8();f.impulse=u8();f.weapon=u8();f.state=u8();f.health=u8();
        f.context.team=u8();f.context.phase=u8();f.context.objective=u8();f.context.friends=u8();f.context.visible=u8();
        f.context.enemyDistance=f32();f.context.enemyYaw=f32();f.context.enemyPitch=f32();f.context.enemy=vec();f.context.targetToken=u32();return f;}
};
bool Database::save(const std::string& path,const std::string& map,uint32_t env,std::string& error) const {
    error.clear();if(map.empty()||map.size()>63){error="invalid map name";return false;}
    Writer w;const char magic[8]={'R','B','C','C','0','0','0','3'};
    w.b.insert(w.b.end(),magic,magic+8);w.u32(3);w.u32(env);w.u32(uint32_t(map.size()));
    w.b.insert(w.b.end(),map.begin(),map.end());w.u32(uint32_t(clips_.size()));
    for(size_t i=0;i<clips_.size();++i){const Clip& c=clips_[i];w.u32(c.id);w.u32(c.profile);w.u32(c.features);w.u32(c.durationMs);
        w.u32(c.successes);w.u32(c.failures);w.vec(c.end);w.u32(uint32_t(c.frames.size()));w.u32(uint32_t(c.throws.size()));
        for(size_t j=0;j<c.frames.size();++j)w.frame(c.frames[j]);
        for(size_t j=0;j<c.throws.size();++j){const Throw& t=c.throws[j];w.u32(t.frame);w.u8(t.weapon);w.u8(t.observed);w.vec(t.landing);}}
    w.u32(hashBytes(w.b.data(),w.b.size()));std::string temp=path+".tmp";
    FILE* fp=std::fopen(temp.c_str(),"wb");if(!fp){error="cannot open temporary dataset";return false;}
    bool ok=std::fwrite(w.b.data(),1,w.b.size(),fp)==w.b.size();if(std::fclose(fp)!=0)ok=false;
    if(!ok){std::remove(temp.c_str());error="dataset write failed";return false;}
#ifdef _WIN32
    ok=MoveFileExA(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    ok=std::rename(temp.c_str(),path.c_str())==0;
#endif
    if(!ok){error="cannot replace dataset; previous file retained";std::remove(temp.c_str());}return ok;
}
bool Database::load(const std::string& path,const std::string& map,uint32_t env,std::string& error) {
    error.clear();FILE* fp=std::fopen(path.c_str(),"rb");if(!fp){error="dataset not found";return false;}
    std::fseek(fp,0,SEEK_END);long size=std::ftell(fp);std::rewind(fp);
    if(size<28||size>32*1024*1024){std::fclose(fp);error="invalid dataset size";return false;}
    std::vector<unsigned char> bytes(static_cast<size_t>(size));bool ok=std::fread(bytes.data(),1,bytes.size(),fp)==bytes.size();std::fclose(fp);
    if(!ok){error="dataset read failed";return false;}
    Reader checksum(bytes);checksum.p=bytes.size()-4;uint32_t expected=checksum.u32();
    if(hashBytes(bytes.data(),bytes.size()-4)!=expected){error="dataset checksum mismatch";return false;}
    Reader r(bytes);for(unsigned i=0;i<8;++i)if(r.u8()!=static_cast<uint8_t>("RBCC0003"[i]))r.ok=false;
    if(r.u32()!=3||r.u32()!=env) {error="dataset version or physics/map fingerprint mismatch";return false;}
    uint32_t length=r.u32();if(length!=map.size()||length>63){error="dataset map mismatch";return false;}
    for(uint32_t i=0;i<length;++i)if(r.u8()!=static_cast<uint8_t>(map[i]))r.ok=false;
    uint32_t count=r.u32();if(count>MaxClips){error="dataset clip limit exceeded";return false;}
    Database next;uint32_t greatest=0;
    for(uint32_t i=0;i<count&&r.ok;++i){Clip c;c.id=r.u32();c.profile=r.u32();c.features=r.u32();c.durationMs=r.u32();c.successes=r.u32();c.failures=r.u32();c.end=r.vec();
        uint32_t frames=r.u32(),throws=r.u32();if(frames>MaxClipFrames||throws>MaxThrows||next.frames_+frames>MaxFrames){r.ok=false;break;}
        for(uint32_t j=0;j<frames&&r.ok;++j)c.frames.push_back(r.frame());
        for(uint32_t j=0;j<throws&&r.ok;++j){Throw t;t.frame=r.u32();t.weapon=r.u8();t.observed=r.u8();t.landing=r.vec();c.throws.push_back(t);}
        if(!r.ok||!c.id||c.id<=greatest||!validClip(c)){r.ok=false;break;}
        greatest=c.id;c.requiredWeapons=0;for(size_t j=0;j<c.frames.size();++j)c.requiredWeapons|=1u<<c.frames[j].weapon;next.frames_+=c.frames.size();next.clips_.push_back(c);
        if(next.models_.size()<65||next.models_.count(c.profile))next.models_[c.profile].learn(c);if(c.profile)next.models_[0].learn(c);
    }
    if(!r.ok||r.p!=bytes.size()-4){error="invalid dataset contents; existing memory retained";return false;}
    next.nextId_=greatest+1;if(!next.nextId_)next.nextId_=1;next.rebuild();
    clips_.swap(next.clips_);index_.swap(next.index_);models_.swap(next.models_);frames_=next.frames_;nextId_=next.nextId_;return true;
}
} // namespace rbcc
