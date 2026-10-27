// GPL-2.0-or-later. No engine access; all traversal must pass a live adapter validator.
#include "copycat_routes.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <queue>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace rbcc {
namespace {
uint32_t randomStep(uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
bool finite(const Vec& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&v.length()<65536;}
void increment(uint32_t& n){if(n<1000000)++n;}
}
std::vector<int> splitRoles(const std::vector<SplitMember>& members,uint32_t seed){
    std::vector<int> result(members.size(),-1);if(members.empty())return result;
    if(!seed)seed=1;
    const int n=int(members.size()), middle=n>=5?std::max(1,n/5):(n>=3?1:0);
    int quota[3]={(n-middle+1)/2,(n-middle)/2,middle};
    std::vector<size_t> order;for(size_t i=0;i<members.size();++i)if(members[i].bomb)order.push_back(i);
    for(size_t i=0;i<members.size();++i)if(!members[i].bomb)order.push_back(i);
    // Shuffle non-carriers, retaining the carrier's first choice of a proper site.
    const size_t first=(!order.empty()&&members[order[0]].bomb)?1:0;
    for(size_t i=order.size();i>first+1;--i)std::swap(order[i-1],order[first+randomStep(seed)%uint32_t(i-first)]);
    for(size_t index:order){int best=-1;float cost=1e9f;
        for(int role=0;role<3;++role){if(quota[role]<=0||(members[index].bomb&&role==2))continue;
            float score=float(randomStep(seed)&65535)/65535.f;
            if(role==members[index].previous)score+=1.4f;
            if(score<cost){best=role;cost=score;}}
        if(best<0)best=0;result[index]=best;--quota[best];
    }return result;
}
void HumanRoutes::clear(){nodes_.clear();dirty_=false;for(auto& t:teachers_)t=Teacher();}
void HumanRoutes::resetTeacher(int slot){if(slot>=0&&slot<32)teachers_[slot]=Teacher();}
int HumanRoutes::nearest(const Vec& pos,float limit,uint8_t team) const{
    int best=-1;float d=limit;
    for(size_t i=0;i<nodes_.size();++i){const auto& n=nodes_[i];if(team<2&&!n.visits[team])continue;
        float candidate=distance(pos,n.origin);if(candidate<d){d=candidate;best=int(i);}}
    return best;
}
int HumanRoutes::addNode(const Frame& f){
    // Do not collapse distinct floors or different stances in a low passage.
    int node=-1;float best=36;
    for(size_t i=0;i<nodes_.size();++i){const auto& n=nodes_[i];
        if(std::fabs(n.origin.z-f.origin.z)>12||(n.state&(Crouched|Ladder|Water))!=(f.state&(Crouched|Ladder|Water)))continue;
        float d=distance(n.origin,f.origin);if(d<best){best=d;node=int(i);}}
    if(node<0){if(nodes_.size()>=MaxNodes)return -1;HumanNode n;n.origin=f.origin;n.look=f.angles;n.state=f.state;nodes_.push_back(n);node=int(nodes_.size()-1);}
    increment(nodes_[size_t(node)].visits[f.context.team]);dirty_=true;return node;
}
void HumanRoutes::observe(const Frame& f,int slot,bool allowHide){
    if(slot<0||slot>=32||!validFrame(f)){resetTeacher(slot);return;}
    auto& t=teachers_[slot];
    if(t.valid&&(distance(t.previous.origin,f.origin)>192||t.previous.context.team!=f.context.team))t=Teacher();
    if(!t.valid){t.valid=true;t.previous=f;t.anchor=addNode(f);t.last=t.anchor;return;}
    const bool stable=(f.state&(Ground|Ladder))!=0;
    if(stable&&(t.anchor<0||distance(nodes_[size_t(t.anchor)].origin,f.origin)>=52||
        (nodes_[size_t(t.anchor)].state&(Crouched|Ladder))!=(f.state&(Crouched|Ladder)))){
        const int node=addNode(f);
        if(node>=0&&t.anchor>=0&&node!=t.anchor){auto& edges=nodes_[size_t(t.anchor)].edges;bool found=false;
            for(auto& e:edges)if(e.to==node){increment(e.visits);e.state|=f.state;found=true;break;}
            if(!found&&edges.size()<MaxLinks)edges.push_back({uint16_t(node),f.state,1});
        }
        t.anchor=node;t.last=node;t.hold=0;t.counted=false;
    }
    if(allowHide&&stable&&!(f.state&Water)&&f.velocity.length()<25&&std::fabs(f.forward)<25&&std::fabs(f.side)<25&&!(f.buttons&(Jump|Use))){
        t.hold=std::min(t.hold+unsigned(f.msec),60000u);
        if(t.anchor>=0&&t.hold>=2000){auto& n=nodes_[size_t(t.anchor)];
            n.holdMs[f.context.team]=std::min(n.holdMs[f.context.team]+unsigned(f.msec),3600000u);
            n.look=f.angles;if(!t.counted){increment(n.hides[f.context.team]);t.counted=true;}dirty_=true;}
    }else{t.hold=0;t.counted=false;}
    t.previous=f;
}
std::vector<uint16_t> HumanRoutes::plan(const Vec& start,const Vec& goal,uint8_t team,uint32_t seed,const Validator& valid) const{
    std::vector<uint16_t> empty;if(team>1||nodes_.empty()||!valid)return empty;
    int src=nearest(start,128,team);if(src<0||!valid(start,nodes_[size_t(src)].origin,nodes_[size_t(src)].state))return empty;
    const float initial=distance(start,goal);if(initial<120)return empty;
    const float infinity=std::numeric_limits<float>::max();std::vector<float> costs(nodes_.size(),infinity);
    std::vector<int> parent(nodes_.size(),-1);
    using Item=std::pair<float,int>;std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
    costs[size_t(src)]=0;queue.push({0,src});int target=-1;float best=infinity;unsigned inspected=0;
    while(!queue.empty()&&inspected++<512){auto current=queue.top();queue.pop();
        if(current.first>costs[size_t(current.second)]+.01f)continue;
        const auto& node=nodes_[size_t(current.second)];float remaining=distance(node.origin,goal);
        if(current.second!=src&&remaining<initial-96){float score=current.first*.32f+remaining;
            if(score<best){best=score;target=current.second;}}
        for(const auto& edge:node.edges){if(edge.to>=nodes_.size())continue;const auto& next=nodes_[edge.to];
            if(!next.visits[team]||!valid(node.origin,next.origin,edge.state))continue;
            uint32_t h=seed^(uint32_t(current.second)*2654435761u)^edge.to;h=hashBytes(&h,sizeof(h));
            const float familiarity=1.f/std::sqrt(float(std::min(edge.visits,100u)));
            float cost=distance(node.origin,next.origin)*(0.8f+float(h&65535)*(.5f/65535.f)+familiarity*.15f+
                std::min(float(next.used[team])*.12f,1.2f));
            float sum=current.first+std::max(1.f,cost);
            if(sum<costs[edge.to]){costs[edge.to]=sum;parent[edge.to]=current.second;queue.push({sum,edge.to});}
        }
    }
    if(target<0)return empty;
    for(int at=target;at>=0&&empty.size()<MaxNodes;at=parent[size_t(at)]){empty.push_back(uint16_t(at));if(at==src)break;}
    if(empty.empty()||empty.back()!=src)return {};
    std::reverse(empty.begin(),empty.end());return empty;
}
int HumanRoutes::favoriteHide(const Vec& centerPoint,uint8_t team,uint32_t seed,float radius) const{
    if(team>1)return -1;int best=-1;float score=-1e9f;
    for(size_t i=0;i<nodes_.size();++i){const auto& n=nodes_[i];if(!n.hides[team]||distance(n.origin,centerPoint)>radius)continue;
        uint32_t h=seed^uint32_t(i)*2654435761u;h=hashBytes(&h,sizeof(h));
        float s=std::log1p(float(n.hides[team]))*3+std::log1p(float(n.holdMs[team])/1000.f)-float(n.used[team])*.4f+
            float(h&65535)/32768.f-distance(n.origin,centerPoint)/600.f;
        if(s>score){score=s;best=int(i);}}
    return best;
}
void HumanRoutes::used(uint16_t i,uint8_t team){if(i<nodes_.size()&&team<2){increment(nodes_[i].used[team]);dirty_=true;}}
void HumanRoutes::newRound(){if(!nodes_.empty())dirty_=true;for(auto& n:nodes_)for(unsigned team=0;team<2;++team)n.used[team]=n.used[team]*3/4;for(auto& t:teachers_)t=Teacher();}
size_t HumanRoutes::edgeCount() const{size_t count=0;for(const auto& n:nodes_)count+=n.edges.size();return count;}
namespace {
struct Bytes{
    std::vector<unsigned char> b;size_t at=0;bool ok=true;
    void put(uint32_t v){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(v>>(i*8)));}
    uint32_t get(){if(at+4>b.size()){ok=false;return 0;}uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(b[at++])<<(i*8);return n;}
    void writeVec(const Vec& v){for(float f:{v.x,v.y,v.z}){uint32_t n;std::memcpy(&n,&f,4);put(n);}}
    Vec readVec(){float f[3];for(auto& v:f){uint32_t n=get();std::memcpy(&v,&n,4);}return {f[0],f[1],f[2]};}
};
}
bool HumanRoutes::save(const std::string& path,const std::string& map,uint32_t env,std::string& error){
    error.clear();if(map.empty()||map.size()>63){error="invalid navigation map";return false;}Bytes w;
    for(unsigned char c:std::string("HCNAV001"))w.b.push_back(c);
    w.put(env);w.put(uint32_t(map.size()));w.b.insert(w.b.end(),map.begin(),map.end());w.put(uint32_t(nodes_.size()));
    for(const auto& n:nodes_){w.writeVec(n.origin);w.writeVec(n.look);w.put(n.state);
        for(unsigned team=0;team<2;++team){w.put(n.visits[team]);w.put(n.hides[team]);w.put(n.holdMs[team]);w.put(n.used[team]);}
        w.put(uint32_t(n.edges.size()));for(const auto& e:n.edges){w.put(e.to);w.put(e.state);w.put(e.visits);}}
    w.put(hashBytes(w.b.data(),w.b.size()));const auto temp=path+".tmp";
    FILE* file=std::fopen(temp.c_str(),"wb");if(!file){error="cannot open human navigation temporary file";return false;}
    bool ok=std::fwrite(w.b.data(),1,w.b.size(),file)==w.b.size();if(std::fclose(file)!=0)ok=false;
#ifdef _WIN32
    if(ok)ok=MoveFileExA(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    if(ok)ok=std::rename(temp.c_str(),path.c_str())==0;
#endif
    if(!ok){std::remove(temp.c_str());error="human navigation write failed; prior file retained";}else dirty_=false;
    return ok;
}
bool HumanRoutes::load(const std::string& path,const std::string& map,uint32_t env,std::string& error){
    error.clear();FILE* file=std::fopen(path.c_str(),"rb");if(!file){error="human navigation not found";return false;}
    std::fseek(file,0,SEEK_END);long size=std::ftell(file);std::rewind(file);
    if(size<24||size>2*1024*1024){std::fclose(file);error="invalid human navigation size";return false;}
    Bytes r;r.b.resize(size_t(size));bool ok=std::fread(r.b.data(),1,r.b.size(),file)==r.b.size();std::fclose(file);
    r.at=r.b.size()-4;uint32_t sum=r.get();r.at=8;
    if(!ok||std::memcmp(r.b.data(),"HCNAV001",8)||hashBytes(r.b.data(),r.b.size()-4)!=sum||r.get()!=env){error="human navigation checksum/version/environment mismatch";return false;}
    uint32_t len=r.get();if(len!=map.size()||len>63||r.at+len>r.b.size()){error="human navigation map mismatch";return false;}
    if(std::memcmp(r.b.data()+r.at,map.data(),len)){error="human navigation map mismatch";return false;}r.at+=len;
    uint32_t count=r.get();if(count>MaxNodes){error="human node limit exceeded";return false;}
    std::vector<HumanNode> next;next.reserve(count);
    for(uint32_t i=0;i<count&&r.ok;++i){HumanNode n;n.origin=r.readVec();n.look=r.readVec();uint32_t state=r.get();
        if(!finite(n.origin)||!finite(n.look)||state>15){r.ok=false;break;}n.state=uint8_t(state);
        for(unsigned t=0;t<2;++t){n.visits[t]=r.get();n.hides[t]=r.get();n.holdMs[t]=r.get();n.used[t]=r.get();
            if(n.visits[t]>1000000||n.hides[t]>1000000||n.holdMs[t]>3600000||n.used[t]>1000000)r.ok=false;}
        uint32_t links=r.get();if(links>MaxLinks){r.ok=false;break;}
        for(uint32_t j=0;j<links&&r.ok;++j){uint32_t to=r.get(),flags=r.get(),visits=r.get();
            if(to>=count||to==i||flags>15||!visits||visits>1000000){r.ok=false;break;}n.edges.push_back({uint16_t(to),uint8_t(flags),visits});}
        next.push_back(n);
    }
    if(!r.ok||r.at!=r.b.size()-4){error="invalid human navigation contents; memory retained";return false;}
    nodes_.swap(next);dirty_=false;for(auto& t:teachers_)t=Teacher();return true;
}
}
