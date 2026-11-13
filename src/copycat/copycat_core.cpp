// Human Copycat extension for RealBot (Level 4: Open-Ended Evolving Skill Library + Causal Attention + Score-Matching Denoising). GPL-2.0-or-later.
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
    const Frame& last=c.frames.back();
    if(last.buttons&(Attack|Attack2|Jump|Duck|Use)||!(last.state&Ground))return false;
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
    v+=(sample-v)/float(std::min(n,uint32_t(500)));
}
SkillModel::SkillModel():reactionMs(240),turnRate(180),aimError(.65f),burstMs(240),pauseMs(180),coverHealth(45),
    aimHeight(0),heightSamples(0),
    reactions(0),turns(0),aims(0),bursts(0),pauses(0),retreats(0),
    strafeCadenceMs(280),peekVelocityRatio(0.72f),counterStrafeSharpness(0.82f),strafeSamples(0) {
    for(unsigned w=0;w<31;++w) { recoil[w]=.45f; recoilSamples[w]=0; }
}
void SkillModel::learn(const Clip& c) {
    unsigned reaction=0,burst=0,pause=0,strafeTimer=0;
    int lastStrafeSign=0;
    bool awaiting=false;
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
            const int sign = f.side > 65.0f ? 1 : (f.side < -65.0f ? -1 : 0);
            strafeTimer += f.msec;
            if(sign != 0 && lastStrafeSign != 0 && sign != lastStrafeSign) {
                if(strafeTimer >= 90 && strafeTimer <= 900)
                    mean(strafeCadenceMs, strafeSamples, clamp(float(strafeTimer), 120.0f, 750.0f));
                const float decelSharpness = clamp(std::fabs(f.side - p.side) / 400.0f, 0.50f, 1.0f);
                mean(counterStrafeSharpness, strafeSamples, decelSharpness);
                strafeTimer = 0;
            }
            if(sign != 0) {
                lastStrafeSign = sign;
                const float ratio = clamp(std::fabs(f.side) / std::max(f.maxSpeed, 100.0f), 0.40f, 1.0f);
                mean(peekVelocityRatio, strafeSamples, ratio);
            }
        }
    }
}

void GenerativeMotion::reset(uint32_t seed) {
    ouOffset = Vec(0, 0, 0);
    history.fill(Vec(0, 0, 0));
    historyCount = 0;
    historyCursor = 0;
    cadencePhase = 0.0f;
    rng = seed ? seed : 0x9e3779b9u;
    denoisedSteps = 0;
}
float GenerativeMotion::randomUniform() {
    if (!rng) rng = 0x9e3779b9u;
    rng ^= rng << 13u;
    rng ^= rng >> 17u;
    rng ^= rng << 5u;
    return float(rng & 65535u) / 32767.5f - 1.0f;
}
void GenerativeMotion::pushHistory(const Vec& motionState) {
    history[historyCursor % HistoryWindow] = motionState;
    historyCursor = (historyCursor + 1u) % HistoryWindow;
    if (historyCount < HistoryWindow) ++historyCount;
}
Vec GenerativeMotion::causalAttentionContext(const Vec& queryState) const {
    if (historyCount == 0) return queryState;
    std::array<float, HistoryWindow> weights {};
    float maxLogit = -1e30f;
    const float invScale = 1.0f / 180.0f;
    for (unsigned i = 0; i < historyCount; ++i) {
        const unsigned idx = (historyCursor + HistoryWindow - 1u - i) % HistoryWindow;
        const Vec& k = history[idx];
        const float dot = (queryState.x * k.x + queryState.y * k.y) * invScale * invScale + (queryState.z * k.z) * 0.02f;
        const float logit = clamp(dot - float(i) * 0.18f, -10.0f, 10.0f);
        weights[i] = logit;
        if (logit > maxLogit) maxLogit = logit;
    }
    float sum = 0.0f;
    for (unsigned i = 0; i < historyCount; ++i) {
        weights[i] = std::exp(weights[i] - maxLogit);
        sum += weights[i];
    }
    if (sum <= 1e-9f) return queryState;
    Vec ctx(0, 0, 0);
    for (unsigned i = 0; i < historyCount; ++i) {
        const unsigned idx = (historyCursor + HistoryWindow - 1u - i) % HistoryWindow;
        const float alpha = weights[i] / sum;
        ctx = ctx + history[idx] * alpha;
    }
    return ctx;
}
Vec GenerativeMotion::denoiseVelocity(const Vec& rawCandidate, const Vec& targetPrior, float sharpness, float dt) {
    Vec x = rawCandidate;
    const Vec attn = causalAttentionContext(targetPrior);
    const Vec manifold = targetPrior * 0.65f + attn * 0.35f;
    const float stepRate = clamp(sharpness * 0.28f, 0.12f, 0.35f);
    for (int step = 0; step < 3; ++step) {
        const Vec scoreGrad = (manifold - x) * stepRate;
        const float noiseScale = (2 - step) * 1.8f * std::sqrt(clamp(dt, 0.001f, 0.05f));
        x.x += scoreGrad.x + randomUniform() * noiseScale;
        x.y += scoreGrad.y + randomUniform() * noiseScale;
        x.z += scoreGrad.z * 0.5f;
    }
    if (denoisedSteps < 1000000000u) ++denoisedSteps;
    pushHistory(x);
    return x;
}
Frame GenerativeMotion::adaptReplayFrame(const Frame& recorded, const Frame& live, const Vec& goal,
    bool hasGoal, const SkillModel& model, float dt, float scale) {
    Frame out = recorded;
    if (scale <= 0.0f || !finite(dt) || dt <= 0.0f || isUtility(recorded.weapon) ||
        (recorded.buttons & (Jump | Use | Attack | Attack2)) || !(recorded.state & Ground) ||
        (recorded.state & (Ladder | Water))) {
        return out;
    }
    dt = clamp(dt, 0.001f, 0.1f);
    scale = clamp(scale, 0.0f, 1.0f);
    const float revert = std::exp(-dt * 4.5f);
    const float sigma = std::sqrt(std::max(0.0f, 1.0f - revert * revert));
    ouOffset.x = ouOffset.x * revert + randomUniform() * sigma * 16.0f * scale;
    ouOffset.y = ouOffset.y * revert + randomUniform() * sigma * 22.0f * scale;
    ouOffset.z = ouOffset.z * revert + randomUniform() * sigma * 3.2f * scale;

    const float maxSpd = std::max(live.maxSpeed, 100.0f);
    const Vec prior(recorded.forward, recorded.side * clamp(model.peekVelocityRatio / 0.72f, 0.85f, 1.15f), 0.0f);
    const Vec noisy(prior.x + ouOffset.x, prior.y + ouOffset.y, ouOffset.z);
    const Vec denoised = denoiseVelocity(noisy, prior, model.counterStrafeSharpness, dt);

    out.forward = clamp(denoised.x, -maxSpd, maxSpd);
    out.side = clamp(denoised.y, -maxSpd, maxSpd);
    if (!live.context.visible && hasGoal) {
        const Vec toGoal = goal - live.origin;
        if (toGoal.length() > 96.0f) {
            const float goalYaw = std::atan2(toGoal.y, toGoal.x) * 57.2957795f;
            const float diff = angleDelta(goalYaw, out.angles.y);
            out.angles.y = angleDelta(out.angles.y + clamp(diff * 0.18f * scale + denoised.z, -8.0f, 8.0f), 0.0f);
        }
    }
    return out;
}
void GenerativeMotion::synthesizeCombatMicro(float dt, const SkillModel& model, float stress,
    float& forwardInOut, float& sideInOut, bool& crouchOut, float crouchLikelihood) {
    if (!finite(dt) || dt <= 0.0f) return;
    dt = clamp(dt, 0.001f, 0.1f);
    stress = clamp(stress, 0.0f, 1.0f);
    const float cadence = clamp(model.strafeCadenceMs, 140.0f, 650.0f) * 0.001f;
    cadencePhase += dt * (6.2831853f / std::max(cadence, 0.14f));
    if (cadencePhase > 62.831853f) cadencePhase -= 62.831853f;

    const float revert = std::exp(-dt * 6.0f);
    const float sigma = std::sqrt(std::max(0.0f, 1.0f - revert * revert));
    ouOffset.x = ouOffset.x * revert + randomUniform() * sigma * (14.0f + 8.0f * stress);
    ouOffset.y = ouOffset.y * revert + randomUniform() * sigma * (18.0f + 10.0f * stress);

    const float wave = 0.88f + 0.12f * std::sin(cadencePhase);
    const Vec prior(forwardInOut, sideInOut * wave, 0.0f);
    const Vec noisy(prior.x + ouOffset.x, prior.y + ouOffset.y, 0.0f);
    const Vec denoised = denoiseVelocity(noisy, prior, model.counterStrafeSharpness, dt);

    sideInOut = clamp(denoised.y, -250.0f, 250.0f);
    forwardInOut = clamp(denoised.x, -250.0f, 250.0f);
    const float roll = (randomUniform() + 1.0f) * 0.5f;
    crouchOut = roll < clamp(crouchLikelihood * dt * 3.5f, 0.0f, 0.25f);
}

// Level 4: Open-Ended Evolving Skill Library Implementation
static uint32_t nextSkillRng(uint32_t& s) {
    if (!s) s = 0x9e3779b9u;
    s ^= s << 13u; s ^= s >> 17u; s ^= s << 5u;
    return s;
}
void EvolvingSkillLibrary::seedFoundationalSkills() {
    skills_.clear();
    nextId_ = 1;
    auto addSeed = [&](const char* name, uint8_t mask, std::initializer_list<SkillPrimitive> steps) {
        SynthesizedSkill sk {};
        sk.id = nextId_++;
        sk.generation = 1;
        sk.fitness = 0.25f;
        sk.contextMask = mask;
        std::snprintf(sk.name, sizeof(sk.name), "%s", name);
        size_t idx = 0;
        for (const auto& st : steps) {
            if (idx < MaxSkillPrimitives) sk.steps[idx++] = st;
        }
        sk.length = static_cast<uint8_t>(std::max<size_t>(1, idx));
        skills_.push_back(sk);
    };
    // 1. Jiggle-Bait Pre-Fire (counter stationary / AWP angles)
    addSeed("seed_jiggle_prefire_g1", CtxSniperBait | CtxMidRifle | CtxEntryPeek, {
        { 0.10f,  0.85f, -4.0f, 0.0f,  85, 0 },
        { 0.00f, -0.95f,  2.5f, 0.0f,  75, Attack },
        {-0.20f, -0.80f,  0.0f, 0.0f,  90, 0 },
        { 0.05f,  0.90f, -2.0f, 0.0f,  85, Attack }
    });
    // 2. Wide-Swing Crouch Transfer (aggressive rifle duel)
    addSeed("seed_wide_crouch_transfer_g1", CtxCloseDuel | CtxMidRifle | CtxRetakeSwing, {
        { 0.25f,  0.95f, -6.0f, 0.0f, 110, 0 },
        { 0.10f,  0.80f, -2.0f, 0.0f,  95, Attack },
        { 0.00f, -0.45f,  0.0f, 0.5f, 110, uint8_t(Duck | Attack) },
        { 0.00f,  0.40f,  1.5f, 0.0f,  95, uint8_t(Duck | Attack) }
    });
    // 3. Counter-Strafe Tap Discipline (long/mid range precision)
    addSeed("seed_counterstrafe_tap_g1", CtxMidRifle | CtxSniperBait, {
        { 0.00f, -0.82f,  3.0f, 0.0f, 100, 0 },
        { 0.00f,  0.75f,  0.0f, 0.0f,  55, Attack },
        { 0.00f,  0.82f, -3.0f, 0.0f, 100, 0 },
        { 0.00f, -0.75f,  0.0f, 0.0f,  55, Attack }
    });
    // 4. Bait-Retreat Re-Peek (low HP survival & angle isolation)
    addSeed("seed_bait_retreat_repeek_g1", CtxLowHpSurvive | CtxCloseDuel, {
        {-0.75f, -0.65f,  5.0f, 0.0f, 110, 0 },
        {-0.55f,  0.70f, -4.0f, 0.0f, 100, Duck },
        { 0.15f,  0.85f, -2.0f, 0.0f,  85, Attack },
        {-0.60f, -0.75f,  0.0f, 0.0f,  95, 0 }
    });
    // 5. Off-Angle Micro-Adjust (entry clearing & multi-angle slicing)
    addSeed("seed_slice_pie_clear_g1", CtxEntryPeek | CtxRetakeSwing, {
        { 0.45f,  0.70f, -12.0f, 0.0f, 95, 0 },
        { 0.20f, -0.35f,   8.0f, 0.0f, 80, 0 },
        { 0.35f, -0.75f,  14.0f, 0.0f, 95, 0 },
        { 0.00f,  0.30f,  -4.0f, 0.0f, 75, Duck }
    });
    // 6. Close-Quarters Evasive Strafe (anti-rush / pistol / close rifle)
    addSeed("seed_evasive_close_duel_g1", CtxCloseDuel | CtxLowHpSurvive, {
        {-0.30f,  0.95f, -8.0f, 0.0f,  90, Attack },
        { 0.20f, -0.95f,  8.0f, 0.0f,  90, uint8_t(Duck | Attack) },
        {-0.25f,  0.88f, -5.0f, 0.0f,  90, Attack },
        { 0.10f, -0.80f,  4.0f, 0.0f,  85, Attack }
    });
}
uint8_t EvolvingSkillLibrary::contextForFrame(const Frame& f) {
    uint8_t mask = 0;
    if (f.health < 45) mask |= CtxLowHpSurvive;
    if (f.context.visible) {
        if (f.context.enemyDistance < 420.0f) mask |= CtxCloseDuel;
        else if (f.context.enemyDistance < 1150.0f) mask |= CtxMidRifle;
        else mask |= CtxSniperBait;
    } else {
        mask |= CtxEntryPeek;
    }
    if (f.context.objective & 1u) mask |= CtxRetakeSwing;
    return mask ? mask : static_cast<uint8_t>(CtxAll);
}
const SynthesizedSkill* EvolvingSkillLibrary::findSkill(uint32_t id) const {
    for (const auto& sk : skills_) if (sk.id == id) return &sk;
    return nullptr;
}
const SynthesizedSkill* EvolvingSkillLibrary::selectSkill(const Frame& f, uint32_t& rngSeed, float exploration) const {
    if (skills_.empty()) return nullptr;
    const uint8_t ctx = contextForFrame(f);
    if (float(nextSkillRng(rngSeed) % 10000u) * 0.0001f < clamp(exploration, 0.0f, 0.35f)) {
        return &skills_[nextSkillRng(rngSeed) % skills_.size()];
    }
    const SynthesizedSkill* best = &skills_[0];
    float highest = -1e30f;
    for (const auto& sk : skills_) {
        const float matchBonus = (sk.contextMask & ctx) ? 0.35f : 0.0f;
        const float ucb = 0.08f / std::sqrt(1.0f + float(sk.executions));
        const float genBonus = std::min(float(sk.generation - 1u) * 0.025f, 0.18f);
        const float jitter = float(nextSkillRng(rngSeed) & 63u) * 0.001f;
        const float score = sk.fitness + matchBonus + ucb + genBonus + jitter;
        if (score > highest) {
            highest = score;
            best = &sk;
        }
    }
    return best;
}
ContinuousMotorCommand EvolvingSkillLibrary::evaluateStep(uint32_t skillId, float elapsedMs, float maxSpeed,
    GenerativeMotion& motion, float dt) const {
    ContinuousMotorCommand cmd {};
    const auto* sk = findSkill(skillId);
    if (!sk || sk->length == 0 || elapsedMs < 0.0f) return cmd;
    float cursorMs = 0.0f;
    size_t stepIdx = sk->length;
    for (size_t i = 0; i < sk->length && i < MaxSkillPrimitives; ++i) {
        const float dur = float(std::max<uint8_t>(40, sk->steps[i].durationMs));
        if (elapsedMs < cursorMs + dur) {
            stepIdx = i;
            break;
        }
        cursorMs += dur;
    }
    if (stepIdx >= sk->length || stepIdx >= MaxSkillPrimitives) return cmd;
    const auto& prim = sk->steps[stepIdx];
    const float spd = clamp(maxSpeed, 100.0f, 260.0f);
    const Vec targetPrior(prim.forwardRatio * spd, prim.sideRatio * spd, prim.yawDelta);
    const Vec denoised = motion.denoiseVelocity(targetPrior, targetPrior, 0.85f, clamp(dt, 0.001f, 0.05f));
    cmd.forward = clamp(denoised.x, -spd, spd);
    cmd.side = clamp(denoised.y, -spd, spd);
    cmd.yawOffset = clamp(prim.yawDelta, -25.0f, 25.0f);
    cmd.pitchOffset = clamp(prim.pitchDelta, -8.0f, 8.0f);
    cmd.buttons = prim.buttons;
    cmd.skillId = sk->id;
    cmd.active = true;
    return cmd;
}
uint32_t EvolvingSkillLibrary::discoverFromFrames(const std::vector<Frame>& frames, float achievedReward) {
    if (frames.size() < 4 || !finite(achievedReward) || achievedReward < 0.20f) return 0;
    SynthesizedSkill sk {};
    sk.id = nextId_++;
    if (!nextId_) nextId_ = 1;
    sk.generation = 2;
    sk.executions = 1;
    sk.successes = 1;
    sk.fitness = clamp(0.28f + achievedReward * 0.25f, 0.15f, 1.50f);
    sk.contextMask = contextForFrame(frames.front());
    const size_t numSteps = std::min<size_t>(MaxSkillPrimitives, std::max<size_t>(3, frames.size() / 2));
    sk.length = static_cast<uint8_t>(numSteps);
    for (size_t s = 0; s < numSteps; ++s) {
        const size_t fIdx = std::min(frames.size() - 1, (s * frames.size()) / numSteps);
        const Frame& f = frames[fIdx];
        const Frame& prev = frames[fIdx > 0 ? fIdx - 1 : 0];
        const float maxSpd = std::max(f.maxSpeed, 100.0f);
        SkillPrimitive p {};
        p.forwardRatio = clamp(f.forward / maxSpd, -1.0f, 1.0f);
        p.sideRatio = clamp(f.side / maxSpd, -1.0f, 1.0f);
        p.yawDelta = clamp(angleDelta(f.angles.y, prev.angles.y), -25.0f, 25.0f);
        p.pitchDelta = clamp(angleDelta(f.angles.x, prev.angles.x), -8.0f, 8.0f);
        p.durationMs = static_cast<uint8_t>(std::clamp<int>(int(f.msec) * 3, 55, 160));
        p.buttons = static_cast<uint8_t>(f.buttons & (Attack | Duck | Jump));
        sk.steps[s] = p;
    }
    if (discoveries_ < 1000000000u) ++discoveries_;
    std::snprintf(sk.name, sizeof(sk.name), "disc_skill_%u_g%u", sk.id, sk.generation);
    if (skills_.size() >= MaxSynthesizedSkills) {
        size_t weakest = 6; // preserve the 6 foundational seeds, prune weakest evolved skill
        if (weakest >= skills_.size()) weakest = 0;
        for (size_t i = weakest + 1; i < skills_.size(); ++i) {
            if (skills_[i].fitness < skills_[weakest].fitness) weakest = i;
        }
        skills_[weakest] = sk;
    } else {
        skills_.push_back(sk);
    }
    dirty_ = true;
    return sk.id;
}
uint32_t EvolvingSkillLibrary::evolveNextGeneration(uint32_t& rngSeed) {
    if (skills_.size() < 2) return 0;
    // Select two high-fitness parent skills via tournament selection
    auto pickParent = [&]() -> const SynthesizedSkill& {
        const size_t a = nextSkillRng(rngSeed) % skills_.size();
        const size_t b = nextSkillRng(rngSeed) % skills_.size();
        return skills_[a].fitness >= skills_[b].fitness ? skills_[a] : skills_[b];
    };
    const auto& p1 = pickParent();
    const auto& p2 = pickParent();
    SynthesizedSkill child = p1;
    child.id = nextId_++;
    if (!nextId_) nextId_ = 1;
    child.generation = std::max(p1.generation, p2.generation) + 1u;
    child.executions = 0;
    child.successes = 0;
    child.fitness = clamp((p1.fitness + p2.fitness) * 0.5f + 0.04f, 0.15f, 1.50f);
    child.contextMask = p1.contextMask | p2.contextMask;
    child.length = std::clamp<uint8_t>((p1.length + p2.length) / 2u, 3u, static_cast<uint8_t>(MaxSkillPrimitives));

    for (uint8_t i = 0; i < child.length; ++i) {
        const SkillPrimitive& src = (i < child.length / 2u) ? p1.steps[i % p1.length] : p2.steps[i % p2.length];
        SkillPrimitive mut = src;
        const float n1 = (float(nextSkillRng(rngSeed) & 1023u) / 511.5f - 1.0f) * 0.18f;
        const float n2 = (float(nextSkillRng(rngSeed) & 1023u) / 511.5f - 1.0f) * 0.22f;
        const float nYaw = (float(nextSkillRng(rngSeed) & 1023u) / 511.5f - 1.0f) * 2.5f;
        mut.forwardRatio = clamp(mut.forwardRatio + n1, -1.0f, 1.0f);
        mut.sideRatio = clamp(mut.sideRatio + n2, -1.0f, 1.0f);
        mut.yawDelta = clamp(mut.yawDelta + nYaw, -25.0f, 25.0f);
        if ((nextSkillRng(rngSeed) % 7u) == 0u) mut.buttons ^= Duck;
        mut.durationMs = static_cast<uint8_t>(std::clamp<int>(int(mut.durationMs) + int(nextSkillRng(rngSeed) % 21u) - 10, 50, 160));
        child.steps[i] = mut;
    }
    if (evolutions_ < 1000000000u) ++evolutions_;
    std::snprintf(child.name, sizeof(child.name), "evo_maneuver_%u_g%u", child.id, child.generation);
    if (skills_.size() >= MaxSynthesizedSkills) {
        size_t weakest = std::min<size_t>(6, skills_.size() - 1);
        for (size_t i = weakest + 1; i < skills_.size(); ++i) {
            if (skills_[i].fitness < skills_[weakest].fitness) weakest = i;
        }
        skills_[weakest] = child;
    } else {
        skills_.push_back(child);
    }
    dirty_ = true;
    return child.id;
}
void EvolvingSkillLibrary::feedback(uint32_t skillId, float reward, uint32_t& rngSeed) {
    if (!skillId || !finite(reward)) return;
    for (auto& sk : skills_) {
        if (sk.id == skillId) {
            if (sk.executions < 1000000000u) ++sk.executions;
            if (reward > 0.05f && sk.successes < 1000000000u) ++sk.successes;
            sk.fitness = clamp(sk.fitness + 0.16f * (clamp(reward, -1.5f, 2.0f) - sk.fitness), -1.0f, 2.0f);
            dirty_ = true;
            if (reward > 0.45f && (sk.executions % 3u == 0u)) {
                evolveNextGeneration(rngSeed);
            }
            return;
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
    if(inspected) *inspected=checked;
    return best;
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
    float persistence=std::exp(-dt/0.19f);
    const float effectiveAimError=std::max(0.32f,m.aimError)*(weapon==18?.24f:1.f)*(1+.65f*stress);
    noise_=noise_*persistence+Vec(random(),random(),0)*(std::sqrt(1-persistence*persistence)*effectiveAimError);
    Vec desired=wanted;
    const bool ak_or_m4 = (weapon == 28 || weapon == 22);
    if(target) {
        desired.x+=noise_.x;desired.y+=noise_.y;
        if(weapon<31) {
            float k=clamp(m.recoilSamples[weapon]?m.recoil[weapon]:.98f,0,1.4f);
            if(weapon==18)k=1;
            else if(ak_or_m4)k=std::max(k,weapon==28?1.08f:1.05f);
            desired.x-=punch.x*k;
            const float yaw_k=ak_or_m4?k*(.90f+.10f*clamp(expertise,0,1)):k*(.75f+.25f*clamp(expertise,0,1));
            desired.y-=punch.y*yaw_k;
        }
    }
    if(target&&reactionLeft_>0)desired=angles_;
    const bool engaging = target != 0 || request || isUtility(weapon);
    float baseSpeed=clamp(m.turnRate,95.f,720.f)*(1-.14f*stress);

    float deltaPitch=angleDelta(desired.x,angles_.x);
    float deltaYaw=angleDelta(desired.y,angles_.y);
    if(!engaging && std::fabs(deltaYaw)>1.2f) {
        const float wristPhase=angles_.y*0.052f+float(rng_&1023u)*0.0061f;
        const float wristArc=clamp(std::fabs(deltaYaw)*0.055f,0.16f,1.65f);
        deltaPitch+=std::sin(wristPhase)*wristArc;
    }
    const float dist2D=std::sqrt(deltaPitch*deltaPitch+deltaYaw*deltaYaw);
    if(dist2D>1e-4f) {
        const float dirX=deltaPitch/dist2D;
        const float dirY=deltaYaw/dist2D;
        const float maxSpeed2D=engaging ? baseSpeed*1.12f : std::min(baseSpeed,340.f);
        const float gain=engaging
            ? (5.8f+2.0f*clamp(dist2D/24.f,0.f,1.f))
            : (4.6f+1.6f*clamp(dist2D/30.f,0.f,1.f));
        const float wishSpeed=maxSpeed2D*std::tanh((dist2D*gain)/std::max(maxSpeed2D,1.f));

        const float arcSign=((rng_&2u)?1.f:-1.f);
        const float ortho=clamp((dist2D-5.f)/42.f,0.f,1.f)*(engaging?0.04f:0.075f)*arcSign;
        const float wishVelX=(dirX-dirY*ortho*0.45f)*wishSpeed;
        const float wishVelY=(dirY+dirX*ortho)*wishSpeed;

        const float response=engaging ? 18.0f : 13.0f;
        const float blend=1.f-std::exp(-dt*response);
        angularVelocity_.x+=(wishVelX-angularVelocity_.x)*blend;
        angularVelocity_.y+=(wishVelY-angularVelocity_.y)*blend;

        const float stepX=angularVelocity_.x*dt;
        const float stepY=angularVelocity_.y*dt;
        const float step2D=std::sqrt(stepX*stepX+stepY*stepY);
        const float softLimit=dist2D*0.90f;
        const float scale=(step2D>softLimit && dist2D>0.f) ? (softLimit/step2D) : 1.f;
        angles_.x=angleDelta(angles_.x+stepX*scale,0);
        angles_.y=angleDelta(angles_.y+stepY*scale,0);
    } else {
        const float decay=std::exp(-dt*16.f);
        angularVelocity_.x*=decay;
        angularVelocity_.y*=decay;
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
bool EvolvingSkillLibrary::save(const std::string& path, const std::string& map, uint32_t env, std::string& error) {
    error.clear();
    if (map.empty() || map.size() > 63) { error = "invalid map name"; return false; }
    Writer w;
    const char magic[8] = {'H','C','S','K','I','L','L','4'};
    w.b.insert(w.b.end(), magic, magic + 8);
    w.u32(env);
    w.u32(uint32_t(map.size()));
    w.b.insert(w.b.end(), map.begin(), map.end());
    w.u32(nextId_);
    w.u32(evolutions_);
    w.u32(discoveries_);
    w.u32(uint32_t(skills_.size()));
    for (const auto& sk : skills_) {
        w.u32(sk.id);
        w.u32(sk.generation);
        w.u32(sk.executions);
        w.u32(sk.successes);
        w.f32(sk.fitness);
        w.u8(sk.contextMask);
        w.u8(sk.length);
        for (size_t i = 0; i < 32; ++i) w.u8(static_cast<unsigned char>(sk.name[i]));
        for (size_t i = 0; i < MaxSkillPrimitives; ++i) {
            w.f32(sk.steps[i].forwardRatio);
            w.f32(sk.steps[i].sideRatio);
            w.f32(sk.steps[i].yawDelta);
            w.f32(sk.steps[i].pitchDelta);
            w.u8(sk.steps[i].durationMs);
            w.u8(sk.steps[i].buttons);
        }
    }
    w.u32(hashBytes(w.b.data(), w.b.size()));
    const std::string temp = path + ".tmp";
    FILE* fp = std::fopen(temp.c_str(), "wb");
    if (!fp) { error = "cannot open skill library temp"; return false; }
    bool ok = std::fwrite(w.b.data(), 1, w.b.size(), fp) == w.b.size();
    if (std::fclose(fp) != 0) ok = false;
    if (!ok) { std::remove(temp.c_str()); error = "skill library write failed"; return false; }
#ifdef _WIN32
    ok = MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename(temp.c_str(), path.c_str()) == 0;
#endif
    if (!ok) { std::remove(temp.c_str()); error = "skill library rename failed"; return false; }
    dirty_ = false;
    return true;
}
bool EvolvingSkillLibrary::load(const std::string& path, const std::string& map, uint32_t env, std::string& error) {
    error.clear();
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) { error = "skill library not found"; return false; }
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::rewind(fp);
    if (sz < 32 || sz > 256 * 1024) { std::fclose(fp); error = "invalid skill library size"; return false; }
    std::vector<unsigned char> bytes(static_cast<size_t>(sz));
    const bool readOk = std::fread(bytes.data(), 1, bytes.size(), fp) == bytes.size();
    std::fclose(fp);
    if (!readOk) { error = "skill library read failed"; return false; }
    Reader tail(bytes); tail.p = bytes.size() - 4;
    if (tail.u32() != hashBytes(bytes.data(), bytes.size() - 4)) { error = "skill library checksum mismatch"; return false; }
    Reader r(bytes);
    for (unsigned i = 0; i < 8; ++i) if (r.u8() != static_cast<uint8_t>("HCSKILL4"[i])) r.ok = false;
    if (!r.ok || r.u32() != env) { error = "skill library environment mismatch"; return false; }
    const uint32_t len = r.u32();
    if (len != map.size() || len > 63) { error = "skill library map mismatch"; return false; }
    for (uint32_t i = 0; i < len; ++i) if (r.u8() != static_cast<uint8_t>(map[i])) r.ok = false;
    const uint32_t nextId = r.u32(), evos = r.u32(), discs = r.u32(), count = r.u32();
    if (!r.ok || count == 0 || count > MaxSynthesizedSkills) { error = "invalid skill count"; return false; }
    std::vector<SynthesizedSkill> cand;
    for (uint32_t i = 0; i < count && r.ok; ++i) {
        SynthesizedSkill sk {};
        sk.id = r.u32();
        sk.generation = r.u32();
        sk.executions = r.u32();
        sk.successes = r.u32();
        sk.fitness = r.f32();
        sk.contextMask = r.u8();
        sk.length = r.u8();
        for (size_t k = 0; k < 32; ++k) sk.name[k] = static_cast<char>(r.u8());
        sk.name[31] = '\0';
        for (size_t k = 0; k < MaxSkillPrimitives; ++k) {
            sk.steps[k].forwardRatio = r.f32();
            sk.steps[k].sideRatio = r.f32();
            sk.steps[k].yawDelta = r.f32();
            sk.steps[k].pitchDelta = r.f32();
            sk.steps[k].durationMs = r.u8();
            sk.steps[k].buttons = r.u8();
        }
        if (!finite(sk.fitness) || sk.length == 0 || sk.length > MaxSkillPrimitives) { r.ok = false; break; }
        cand.push_back(sk);
    }
    if (!r.ok || r.p != bytes.size() - 4) { error = "invalid skill library payload"; return false; }
    skills_ = std::move(cand);
    nextId_ = std::max<uint32_t>(1u, nextId);
    evolutions_ = evos;
    discoveries_ = discs;
    dirty_ = false;
    return true;
}
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
        greatest=c.id;c.requiredWeapons=0;for(size_t j=0;j<c.frames.size();++j)c.requiredWeapons|=1u<<c.frames[i].weapon;next.frames_+=c.frames.size();next.clips_.push_back(c);
        if(next.models_.size()<65||next.models_.count(c.profile)) next.models_[c.profile].learn(c);
        if(c.profile) next.models_[0].learn(c);
    }
    if(!r.ok||r.p!=bytes.size()-4){error="invalid dataset contents; existing memory retained";return false;}
    next.nextId_=greatest+1;if(!next.nextId_)next.nextId_=1;next.rebuild();
    clips_.swap(next.clips_);index_.swap(next.index_);models_.swap(next.models_);frames_=next.frames_;nextId_=next.nextId_;return true;
}
} // namespace rbcc
