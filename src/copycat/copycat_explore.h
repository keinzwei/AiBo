// Geometry-discovered navigation overlay. GPL-2.0-or-later.
#pragma once
#include "copycat_core.h"
#include <array>
#include <functional>
#include <map>
#include <tuple>

namespace rbcc {
struct ExploreLink { uint16_t to {}; uint8_t posture {}; };
struct ExploreNode {
  Vec foot;
  std::vector<ExploreLink> links {};
  uint32_t visits[2] {}, lastVisit[2] {};
  uint8_t tested {}, posture {}, vantage {};
};
struct ExploreStep { Vec foot; uint8_t posture {}; bool reverse {true}; uint8_t vantage {}; };
struct ExplorePlan { std::vector<uint16_t> path {}; int goal {-1}; unsigned examined {}; };
// The probe must validate hull clearance and ground along the ENTIRE segment.
// Node positions are feet, independent of standing/crouching origin offsets.
class ExploreMap final {
  std::vector<ExploreNode> nodes_ {};
  std::map<std::tuple<int,int,int>, std::vector<uint16_t>> index_ {};
  std::array<int, 32> reservations_ {};
  std::array<uint8_t, 32> reservation_teams_ {};
  size_t scan_ {}; unsigned direction_ {}; uint32_t clock_ {}; bool dirty_ {};
  std::tuple<int,int,int> key (const Vec &p) const;
  void rebuild ();
public:
  static constexpr size_t MaxNodes = 16384, MaxLinks = 16;
  static constexpr float Spacing = 64.0f;
  using Probe = std::function<bool (const Vec &, const Vec &, ExploreStep &)>;
  ExploreMap () { reservations_.fill (-1); reservation_teams_.fill (255); }
  void clear ();
  int nearest (const Vec &p, float radius = 42.0f) const;
  int seed (const Vec &foot, uint8_t posture = 0);
  bool link (int from, int to, uint8_t posture, bool reverse = true);
  unsigned expand (unsigned budget, const Probe &probe, const std::function<bool ()> &expired = {});
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
