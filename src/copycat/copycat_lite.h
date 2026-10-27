// Small bounded policies; GPL-2.0-or-later. No model/runtime dependency.
#ifndef RB_COPYCAT_LITE_H
#define RB_COPYCAT_LITE_H
#include <stdint.h>
#include <stddef.h>
namespace rbcc {
class LiteRng {
    uint32_t state_;
public:
    explicit LiteRng(uint32_t seed=1):state_(seed?seed:1){}
    uint32_t next();
    float unit();
    unsigned bounded(unsigned count);
};
int weightedChoice(const float* weights,size_t count,LiteRng& rng);
int preferredPrimary(int team,int money,int current,unsigned snipers,unsigned teammates,
                     float sniperTaste,LiteRng& rng);
int preferredSecondary(int money,int current,bool hasPrimary,LiteRng& rng);
bool preferredGun(unsigned weapon);
struct GunTuning {
    float burstMs,pauseMs,maxFireSpeed,maxError;
    bool tap,scope;
};
GunTuning gunTuning(unsigned weapon,float distance);
enum LiteChatEvent { ChatRound,ChatReply,ChatDeath,ChatUtility,ChatHold };
void composeChat(char* out,size_t capacity,LiteChatEvent event,LiteRng& rng);
void composeName(char* out,size_t capacity,unsigned identity,LiteRng& rng);
unsigned desiredBots(unsigned quota,unsigned clients,unsigned otherClients);
} // namespace rbcc
#endif
