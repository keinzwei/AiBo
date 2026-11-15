// Geometry-discovered navigation overlay + Level 2 Graph Attention Network (GAT). GPL-2.0-or-later.
#pragma once
#include "copycat_core.h"
#include <array>
#include <functional>
#include <map>
#include <tuple>
#include <vector>

namespace rbcc {
constexpr size_t GatFeatureDim = 8;
struct ExploreLink { uint16_t to {}; uint8_t posture {}; };
struct GatNodeState {
  float ambushScore {};
  float exposureScore {};
  float learnedValue[2] {};
  bool ready {};
};
struct ExploreNode {
  Vec foot;
  std::vector<ExploreLink> links {};
  uint32_t visits[2] {}, lastVisit[2] {};
  uint8_t tested {}, posture {}, vantage {}, hide {}, crouchCover {};
  int16_t watchYaw {};
  GatNodeState gat {};
};
struct ExploreStep {
  Vec foot;
  uint8_t posture {};
  bool reverse {true};
  uint8_t vantage {};
  uint8_t hide {};
  uint8_t crouchCover {};
  int16_t watchYaw {};
};
struct ExplorePlan { std::vector<uint16_t> path {}; int goal {-1}; unsigned examined {}; };
struct GraphAttentionWeights {
  std::array<std::array<float, GatFeatureDim>, GatFeatureDim> w {};
  std::array<float, GatFeatureDim> aSrc {}, aDst {};
  std::array<float, GatFeatureDim> readout[2] {};
  uint32_t updates {};
};
// The probe must validate hull clearance and ground along the ENTIRE segment.
// Node positions are feet, independent of standing/crouching origin offsets.
class ExploreMap final {
  std::vector<ExploreNode> nodes_ {};
  std::map<std::tuple<int,int,int>, std::vector<uint16_t>> index_ {};
  std::array<int, 32> reservations_ {};
  std::array<uint8_t, 32> reservation_teams_ {};
  GraphAttentionWeights gat_ {};
  size_t scan_ {}; unsigned direction_ {}; uint32_t clock_ {}; bool dirty_ {};
  std::tuple<int,int,int> key (const Vec &p) const;
  void rebuild ();
  void initGat ();
  std::array<float, GatFeatureDim> nodeFeatures (size_t idx, unsigned team) const;
public:
  static constexpr size_t MaxNodes = 16384, MaxLinks = 16;
  static constexpr float Spacing = 64.0f;
  using Probe = std::function<bool (const Vec &, const Vec &, ExploreStep &)>;
  ExploreMap () { reservations_.fill (-1); reservation_teams_.fill (255); initGat (); }
  void clear ();
  int nearest (const Vec &p, float radius = 42.0f) const;
  int seed (const Vec &foot, uint8_t posture = 0);
  void annotate (int node, uint8_t vantage, uint8_t hide, uint8_t crouchCover, int16_t watchYaw);
  bool link (int from, int to, uint8_t posture, bool reverse = true);
  unsigned expand (unsigned budget, const Probe &probe, const std::function<bool ()> &expired = {});
  void updateNodeAttention (int node);
  float gatScore (int node, unsigned team, bool sniper = false) const;
  void reinforceNode (int node, unsigned team, float reward);
  uint32_t gatUpdates () const { return gat_.updates; }
  ExplorePlan plan (const Vec &foot, unsigned team, unsigned slot, uint32_t seed, bool sniper, unsigned budget = 12000,
    const std::function<float (const Vec &)> &goal_bias = {}, const std::function<bool (const Vec &)> &fresh = {});
  void visit (int node, unsigned team);
  void release (unsigned slot);
  void block (int from, int to);
  void retry (int node);
  void newRound ();
  const std::vector<ExploreNode> &nodes () const { return nodes_; }
  size_t edges () const;
  bool dirty () const { return dirty_; }
  bool save (const std::string &path, const std::string &map, uint32_t env, std::string &error);
  bool load (const std::string &path, const std::string &map, uint32_t env, std::string &error);
};
} // namespace rbcc
