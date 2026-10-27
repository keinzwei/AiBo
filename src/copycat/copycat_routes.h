// GPL-2.0-or-later. Additive navigation learned exclusively from human commands.
#ifndef RBCC_ROUTES_H
#define RBCC_ROUTES_H
#include "copycat_core.h"
#include <functional>
#include <vector>
namespace rbcc {
struct HumanEdge { uint16_t to; uint8_t state; uint32_t visits; };
struct HumanNode {
    Vec origin, look;
    uint32_t visits[2], hides[2], holdMs[2], used[2];
    uint8_t state;
    std::vector<HumanEdge> edges;
    HumanNode():visits{0,0},hides{0,0},holdMs{0,0},used{0,0},state(0){}
};
struct SplitMember { int slot, previous; bool bomb; };
// Roles 0/1 are the two objective wings; role 2 probes a distinct middle approach.
std::vector<int> splitRoles(const std::vector<SplitMember>& members,uint32_t seed);
class HumanRoutes {
    struct Teacher {
        int last, anchor; unsigned hold; bool valid, counted; Frame previous;
        Teacher():last(-1),anchor(-1),hold(0),valid(false),counted(false){}
    } teachers_[32];
    std::vector<HumanNode> nodes_;
    bool dirty_;
    int nearest(const Vec& position,float limit,uint8_t team) const;
    int addNode(const Frame& frame);
public:
    static const size_t MaxNodes=2048, MaxLinks=12;
    using Validator=std::function<bool(const Vec&,const Vec&,uint8_t)>;
    HumanRoutes():dirty_(false){}
    void clear();
    void resetTeacher(int slot);
    void observe(const Frame& frame,int slot,bool allowHide);
    std::vector<uint16_t> plan(const Vec& start,const Vec& goal,uint8_t team,uint32_t seed,const Validator& valid) const;
    int favoriteHide(const Vec& centerPoint,uint8_t team,uint32_t seed,float radius=600) const;
    void used(uint16_t node,uint8_t team);
    void newRound();
    bool save(const std::string& path,const std::string& map,uint32_t environment,std::string& error);
    bool load(const std::string& path,const std::string& map,uint32_t environment,std::string& error);
    const std::vector<HumanNode>& nodes() const {return nodes_;}
    bool dirty() const {return dirty_;}
    size_t edgeCount() const;
};
}
#endif
