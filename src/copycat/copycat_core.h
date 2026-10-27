// Human Copycat extension for RealBot. GPL-2.0-or-later; see COPYING.
#ifndef RB_COPYCAT_CORE_H
#define RB_COPYCAT_CORE_H
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <string>
#include <vector>

namespace rbcc {
static const size_t MaxClips = 1024, MaxFrames = 131072, MaxClipFrames = 4096;
static const unsigned MaxClipMs = 8000, MaxThrows = 4;
enum Button { Attack=1, Jump=2, Duck=4, Use=32, Attack2=2048, Reload=8192 };
enum State { Ground=1, Crouched=2, Ladder=4, Water=8 };
enum Feature { Navigation=1, Jumping=2, DoubleDuck=4, DuckCadence=8,
    StrafeJump=16, HE=32, Flash=64, Smoke=128, Combat=256, Holding=512,
    Retreat=1024, Reloading=2048, Peeking=4096, Interaction=8192 };
static const unsigned Utility = HE | Flash | Smoke;
struct Vec {
    float x,y,z;
    Vec(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
    Vec operator-(const Vec& b) const { return Vec(x-b.x,y-b.y,z-b.z); }
    Vec operator+(const Vec& b) const { return Vec(x+b.x,y+b.y,z+b.z); }
    Vec operator*(float f) const { return Vec(x*f,y*f,z*f); }
    float length() const;
};
float distance(const Vec& a,const Vec& b);
float angleDelta(float a,float b);
bool isUtility(unsigned weapon);
struct Context {
    uint8_t team, phase, objective, friends, visible;
    float enemyDistance, enemyYaw, enemyPitch;
    Vec enemy;
    uint32_t targetToken;
    Context():team(255),phase(0),objective(0),friends(0),visible(0),enemyDistance(0),enemyYaw(0),enemyPitch(0),targetToken(0){}
};
struct Frame {
    Vec origin, velocity, angles, punch;
    float forward, side, up, maxSpeed;
    uint16_t buttons, previousButtons;
    uint8_t msec, impulse, weapon, state, health;
    Context context;
    Frame():forward(0),side(0),up(0),maxSpeed(250),buttons(0),previousButtons(0),msec(0),impulse(0),weapon(0),state(0),health(100){}
};
struct Throw {
    uint32_t frame;
    uint8_t weapon, observed;
    Vec landing;
    Throw():frame(0),weapon(0),observed(0){}
};
struct Clip {
    uint32_t id, profile, features, durationMs, successes, failures, requiredWeapons;
    Vec end;
    std::vector<Frame> frames;
    std::vector<Throw> throws;
    Clip():id(0),profile(0),features(0),durationMs(0),successes(0),failures(0),requiredWeapons(0){}
};
struct Query {
    Frame current;
    Vec goal;
    uint32_t weapons, profile, excludeId, selectionSeed;
    bool hasGoal, utility, combat;
    size_t candidates;
    Query():weapons(0),profile(0),excludeId(0),selectionSeed(0),hasGoal(false),utility(true),combat(true),candidates(128){}
};
struct Cell {
    int x,y,z;
    bool operator<(const Cell& b) const;
};
struct SkillModel {
    float reactionMs, turnRate, aimError, burstMs, pauseMs, coverHealth;
    float aimHeight;
    uint32_t heightSamples;
    float recoil[31];
    uint32_t reactions, turns, aims, bursts, pauses, retreats, recoilSamples[31];
    SkillModel();
    void learn(const Clip& clip);
};
class Database {
    std::vector<Clip> clips_;
    std::map<Cell,std::vector<size_t> > index_;
    size_t frames_;
    uint32_t nextId_;
    std::map<uint32_t,SkillModel> models_;
    void rebuild();
public:
    Database():frames_(0),nextId_(1){}
    void clear();
    uint32_t add(Clip clip);
    const Clip* match(const Query& query, size_t* inspected=0) const;
    void feedback(uint32_t id, bool success);
    const SkillModel& model(uint32_t profile=0) const;
    bool save(const std::string& path,const std::string& map,uint32_t environment,std::string& error) const;
    bool load(const std::string& path,const std::string& map,uint32_t environment,std::string& error);
    size_t size() const { return clips_.size(); }
    size_t frameCount() const { return frames_; }
    const std::vector<Clip>& clips() const { return clips_; }
};
class Recorder {
    Clip pending_;
    uint16_t lastButtons_;
    unsigned sinceRelease_, quietMs_;
    bool valid_, haveLast_;
    Frame last_;
    void start(uint32_t profile);
public:
    Recorder():lastButtons_(0),sinceRelease_(0),quietMs_(0),valid_(false),haveLast_(false){}
    void reset();
    // Input is the state BEFORE this command, not an interpolated server snapshot.
    uint32_t capture(const Frame& frame,uint32_t profile,Database& database);
    void noteLanding(uint8_t weapon,const Vec& position);
    size_t pendingFrames() const { return pending_.frames.size(); }
};
class Playback {
    Clip clip_;
    size_t cursor_;
    double credit_;
    bool active_;
public:
    Playback():cursor_(0),credit_(0),active_(false){}
    bool start(const Clip& clip);
    bool advance(double elapsedMs);
    const Frame* due() const;
    void consume();
    void pauseTime() { credit_=0; }
    void stop();
    bool active() const { return active_; }
    bool finished() const { return cursor_>=clip_.frames.size(); }
    size_t cursor() const { return cursor_; }
    uint32_t id() const { return clip_.id; }
    const Clip& clip() const { return clip_; }
};
struct AimOutput { Vec angles; bool fire; };
class HumanAim {
    Vec angles_, angularVelocity_, noise_;
    float reactionLeft_, burstLeft_, pauseLeft_;
    uint32_t target_, rng_;
    uint8_t lastWeapon_;
    bool initialized_, shooting_;
    float random();
public:
    HumanAim():reactionLeft_(0),burstLeft_(0),pauseLeft_(0),target_(0),rng_(1),lastWeapon_(0),initialized_(false),shooting_(false){}
    void reset(const Vec& angles,uint32_t seed);
    AimOutput update(float dt,const Vec& desired,const Vec& punch,uint8_t weapon,
        uint32_t visibleTarget,bool requestFire,float stress,const SkillModel& model,
        float enemyDistance=0,float moveSpeed=0,bool scoped=true,bool weaponPolicy=false,float expertise=0,bool externalDiscipline=false);
};
class HumanStress {
    float value_;
    int health_;
public:
    HumanStress():value_(0),health_(100){}
    void reset() { value_=0; health_=100; }
    float update(float dt,int health,bool enemyVisible,unsigned friends,bool reloading);
    bool wantsCover(const SkillModel& model,int health) const;
    float value() const { return value_; }
};
bool validFrame(const Frame& f);
bool validClip(const Clip& c);
uint32_t analyze(const std::vector<Frame>& frames);
uint32_t hashBytes(const void* data,size_t length,uint32_t hash=2166136261u);
} // namespace rbcc
#endif
