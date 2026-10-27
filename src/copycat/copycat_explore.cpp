// Geometry-discovered navigation overlay. GPL-2.0-or-later.
#include "copycat_explore.h"
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
bool validPoint (const Vec &p) { return std::isfinite (p.x) && std::isfinite (p.y) && std::isfinite (p.z) && std::fabs (p.x) < 65536 && std::fabs (p.y) < 65536 && std::fabs (p.z) < 65536; }
bool validMap (const std::string &m) { return !m.empty () && m.size () <= 63 && m.find_first_not_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos; }
void put (std::vector<uint8_t> &b, uint32_t n) { for (unsigned i = 0; i < 4; ++i) b.push_back (static_cast<uint8_t> (n >> (8u * i))); }
void real (std::vector<uint8_t> &b, float f) { uint32_t n; std::memcpy (&n, &f, 4); put (b, n); }
struct Read {
  const std::vector<uint8_t> &b; size_t pos {}; bool ok {true};
  uint32_t get () { if (pos + 4 > b.size ()) { ok = false; return 0; } uint32_t n {}; for (unsigned i = 0; i < 4; ++i) n |= static_cast<uint32_t> (b[pos++]) << (i * 8u); return n; }
  float real () { const auto n = get (); float f; std::memcpy (&f, &n, 4); return f; }
};
}
std::tuple<int,int,int> ExploreMap::key (const Vec &p) const { return {static_cast<int> (std::floor (p.x / 64)), static_cast<int> (std::floor (p.y / 64)), static_cast<int> (std::floor (p.z / 32))}; }
void ExploreMap::rebuild () { index_.clear (); for (size_t i = 0; i < nodes_.size (); ++i) index_[key (nodes_[i].foot)].push_back (static_cast<uint16_t> (i)); }
void ExploreMap::clear () { *this = ExploreMap {}; }
int ExploreMap::nearest (const Vec &p, float radius) const {
  if (!validPoint (p) || !std::isfinite (radius) || radius <= 0 || radius > 512) return -1;
  const auto [x,y,z] = key (p); const int xy = static_cast<int> (std::ceil (radius / 64));
  const int zz = static_cast<int> (std::ceil (radius / 32)); float best = radius; int result = -1;
  for (int a = -xy; a <= xy; ++a) for (int b = -xy; b <= xy; ++b) for (int c = -zz; c <= zz; ++c) {
    const auto it = index_.find ({x+a,y+b,z+c}); if (it == index_.end ()) continue;
    for (auto n : it->second) {
      if (std::fabs (nodes_[n].foot.z - p.z) > 24.0f) continue; // never merge stacked floors
      const float d = distance (p, nodes_[n].foot); if (d < best) { best = d; result = n; }
    }
  }
  return result;
}
int ExploreMap::seed (const Vec &p, uint8_t posture) {
  if (!validPoint (p) || posture > 1) return -1;
  const int old = nearest (p, 34); if (old >= 0) return old;
  if (nodes_.size () >= MaxNodes) return -1;
  const auto n = static_cast<uint16_t> (nodes_.size ()); ExploreNode node; node.foot = p; node.posture = posture;
  nodes_.push_back (node); index_[key (p)].push_back (n); dirty_ = true; return n;
}
bool ExploreMap::link (int from, int to, uint8_t posture, bool reverse) {
  if (from < 0 || to < 0 || from == to || static_cast<size_t> (from) >= nodes_.size () || static_cast<size_t> (to) >= nodes_.size () || posture > 1 || distance (nodes_[from].foot, nodes_[to].foot) > 128) return false;
  const auto add = [&] (int a, int b) {
    auto &links = nodes_[a].links;
    for (auto &l : links) if (l.to == b) { l.posture = posture; return true; }
    if (links.size () >= MaxLinks) return false;
    links.push_back ({static_cast<uint16_t> (b), posture}); dirty_ = true; return true;
  };
  const bool result = add (from, to); if (reverse) add (to, from); return result;
}
unsigned ExploreMap::expand (unsigned budget, const Probe &probe, const std::function<bool ()> &expired) {
  if (nodes_.empty () || !probe) return 0;
  static constexpr int offsets[8][2] = {{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{-1,-1},{1,-1}};
  unsigned done = 0; size_t skipped = 0;
  while (done < budget && skipped < nodes_.size () * 8u && (!expired || !expired ())) {
    if (scan_ >= nodes_.size ()) scan_ = 0;
    const size_t n = scan_; const unsigned d = direction_++;
    if (direction_ == 8) { direction_ = 0; ++scan_; }
    if (nodes_[n].tested & (1u << d)) { ++skipped; continue; }
    nodes_[n].tested |= static_cast<uint8_t> (1u << d); dirty_ = true; ++done;
    const Vec from = nodes_[n].foot;
    const Vec candidate = from + Vec (static_cast<float> (offsets[d][0]) * Spacing, static_cast<float> (offsets[d][1]) * Spacing, 0);
    ExploreStep step;
    if (!probe (from, candidate, step) || !validPoint (step.foot) || step.posture > 1 || distance (candidate, step.foot) > 96) continue;
    const int next = seed (step.foot, step.posture); if (next < 0) continue;
    nodes_[static_cast<size_t> (next)].vantage = std::max (nodes_[static_cast<size_t> (next)].vantage, step.vantage);
    link (static_cast<int> (n), next, step.posture, step.reverse);
  }
  return done;
}
ExplorePlan ExploreMap::plan (const Vec &foot, unsigned team, unsigned slot, uint32_t seedValue, bool sniper, unsigned budget,
    const std::function<float (const Vec &)> &goal_bias,const std::function<bool (const Vec &)> &fresh) {
  ExplorePlan result; if (team > 1 || slot >= reservations_.size ()) return result;
  const int start = nearest (foot, 90); if (start < 0) return result;
  const size_t count = nodes_.size (); std::vector<float> costs (count, std::numeric_limits<float>::infinity ());
  std::vector<int> parent (count, -1); using Entry = std::pair<float,uint16_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
  costs[start] = 0; open.push ({0, static_cast<uint16_t> (start)});
  int best = -1, fallback = -1; float highest = -1e30f, fallback_score = -1e30f;
  while (!open.empty () && result.examined < std::min (budget, static_cast<unsigned> (MaxNodes))) {
    const auto [cost, n] = open.top (); open.pop (); if (cost > costs[n]) continue; ++result.examined;
    const auto &node = nodes_[n]; float separation = 0;
    for (size_t s = 0; s < reservations_.size (); ++s) if (s != slot && reservations_[s] >= 0 && reservation_teams_[s] == team) {
      const float d = distance (node.foot, nodes_[static_cast<size_t> (reservations_[s])].foot);
      if (d < 280) separation += (280 - d) * 2.0f;
    }
    uint32_t mix = seedValue ^ static_cast<uint32_t> (n) * 2654435761u; mix = hashBytes (&mix, sizeof mix);
    const float jitter = static_cast<float> (mix & 255u) * 0.16f;
    const float age = static_cast<float> (std::min (clock_ - node.lastVisit[team], 1000u));
    float score = (node.visits[team] == 0 ? 420.0f : 180.0f / std::sqrt (static_cast<float> (node.visits[team]))) +
      (node.tested != 255 ? 50.0f : 0.0f) + age * 0.15f + jitter - cost * 0.035f - separation;
    if (sniper) score += static_cast<float> (node.vantage) * 0.55f;
    if (goal_bias) score += goal_bias (node.foot);
    if (n != start && cost >= 48) {
      if (score > fallback_score) { fallback_score = score; fallback = n; }
      if ((!fresh || fresh (node.foot)) && score > highest) { highest = score; best = n; }
    }
    for (const auto &edge : node.links) {
      uint32_t random = seedValue ^ static_cast<uint32_t> (n) * 2246822519u ^ static_cast<uint32_t> (edge.to) * 3266489917u;
      random = hashBytes (&random,sizeof random);
      const float variety = goal_bias ? static_cast<float> (random & 255u) * (0.25f / 255.0f) : 0.0f;
      const float nextCost = cost + distance (node.foot, nodes_[edge.to].foot) * (1.0f + variety) + (edge.posture ? 20.0f : 0.0f);
      if (nextCost < costs[edge.to]) { costs[edge.to] = nextCost; parent[edge.to] = n; open.push ({nextCost, edge.to}); }
    }
  }
  if (best < 0) best = fallback;
  if (best < 0) return result;
  for (int n = best; n >= 0; n = parent[static_cast<size_t> (n)]) {
    result.path.push_back (static_cast<uint16_t> (n)); if (n == start) break;
    if (result.path.size () > MaxNodes) { result.path.clear (); return result; }
  }
  std::reverse (result.path.begin (), result.path.end ()); result.goal = best; reservations_[slot] = best; reservation_teams_[slot] = static_cast<uint8_t> (team); return result;
}
void ExploreMap::visit (int n, unsigned team) {
  if (n < 0 || static_cast<size_t> (n) >= nodes_.size () || team > 1) return;
  if (clock_ < 1000000000u) ++clock_;
  auto &node = nodes_[static_cast<size_t> (n)]; if (node.visits[team] < 1000000000u) ++node.visits[team]; node.lastVisit[team] = clock_; dirty_ = true;
}
void ExploreMap::release (unsigned slot) { if (slot < reservations_.size ()) { reservations_[slot] = -1; reservation_teams_[slot] = 255; } }
void ExploreMap::retry (int node) { if (node >= 0 && static_cast<size_t> (node) < nodes_.size ()) { nodes_[static_cast<size_t> (node)].tested = 0; scan_ = static_cast<size_t> (node); direction_ = 0; } }
void ExploreMap::block (int from, int to) {
  if (from < 0 || static_cast<size_t> (from) >= nodes_.size ()) return;
  auto &links = nodes_[static_cast<size_t> (from)].links;
  links.erase (std::remove_if (links.begin (), links.end (), [to] (const ExploreLink &l) { return l.to == to; }), links.end ());
  dirty_ = true;
}
void ExploreMap::newRound () {
  reservations_.fill (-1); reservation_teams_.fill (255); for (auto &n : nodes_) n.tested = 0; scan_ = 0; direction_ = 0; dirty_ = !nodes_.empty ();
}
size_t ExploreMap::edges () const { size_t count {}; for (const auto &n : nodes_) count += n.links.size (); return count; }
bool ExploreMap::save (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear (); if (!validMap (map)) { error = "invalid map"; return false; }
  std::vector<uint8_t> b {'H','C','E','X','P','L','0','1'}; put (b, env); put (b, static_cast<uint32_t> (map.size ()));
  b.insert (b.end (), map.begin (), map.end ()); put (b, static_cast<uint32_t> (nodes_.size ())); put (b, clock_);
  for (const auto &n : nodes_) {
    real (b, n.foot.x); real (b, n.foot.y); real (b, n.foot.z);
    for (unsigned t = 0; t < 2; ++t) { put (b, n.visits[t]); put (b, n.lastVisit[t]); }
    put (b, n.posture); put (b, n.vantage); put (b, static_cast<uint32_t> (n.links.size ()));
    for (const auto &l : n.links) { put (b, l.to); put (b, l.posture); }
  }
  put (b, hashBytes (b.data (), b.size ()));
  const auto temp = path + ".tmp"; auto f = std::fopen (temp.c_str (), "wb");
  if (!f) { error = "cannot open exploration temp file"; return false; }
  bool ok = std::fwrite (b.data (), 1, b.size (), f) == b.size ();
  if (std::fflush (f) != 0) ok = false;
  if (std::fclose (f) != 0) ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA (temp.c_str (), path.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename (temp.c_str (), path.c_str ()) == 0;
#endif
  }
  if (!ok) { std::remove (temp.c_str ()); error = "exploration save failed; previous file retained"; return false; }
  dirty_ = false; return true;
}
bool ExploreMap::load (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear (); auto f = std::fopen (path.c_str (), "rb"); if (!f) { error = "exploration file not found"; return false; }
  std::vector<uint8_t> b (3 * 1024 * 1024); const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool failed = std::ferror (f) != 0; std::fclose (f);
  if (failed || size < 28 || size >= b.size () || !validMap (map)) { error = "invalid exploration length"; return false; }
  b.resize (size); Read r {b}; Read tail {b}; tail.pos = size - 4;
  if (std::memcmp (b.data (), "HCEXPL01", 8) || tail.get () != hashBytes (b.data (), size - 4)) { error = "exploration header/checksum mismatch"; return false; }
  r.pos = 8; const auto stored = r.get (), mapSize = r.get ();
  if (stored != env || mapSize != map.size () || 16 + mapSize + 12 > size || std::memcmp (b.data () + 16, map.data (), mapSize)) { error = "exploration map/physics mismatch"; return false; }
  r.pos += mapSize; const auto count = r.get (); const auto clock = r.get ();
  if (count > MaxNodes || clock > 1000000000u || count * 40u > size) { error = "invalid exploration count"; return false; }
  ExploreMap candidate; candidate.clock_ = clock; candidate.nodes_.resize (count);
  for (size_t i = 0; i < count; ++i) {
    auto &n = candidate.nodes_[i]; n.foot.x = r.real (); n.foot.y = r.real (); n.foot.z = r.real ();
    for (unsigned t = 0; t < 2; ++t) { n.visits[t] = r.get (); n.lastVisit[t] = r.get (); }
    const auto posture = r.get (), vantage = r.get (), links = r.get ();
    if (!r.ok || !validPoint (n.foot) || posture > 1 || vantage > 255 || links > MaxLinks ||
        n.visits[0] > 1000000000u || n.visits[1] > 1000000000u || n.lastVisit[0] > clock || n.lastVisit[1] > clock) { error = "invalid exploration node"; return false; }
    n.posture = static_cast<uint8_t> (posture); n.vantage = static_cast<uint8_t> (vantage);
    for (size_t j = 0; j < links; ++j) {
      const auto to = r.get (), flags = r.get (); bool duplicate = false; for (const auto &l : n.links) if (l.to == to) duplicate = true;
      if (!r.ok || to >= count || to == i || flags > 1 || duplicate) { error = "invalid exploration edge"; return false; }
      n.links.push_back ({static_cast<uint16_t> (to), static_cast<uint8_t> (flags)});
    }
  }
  if (!r.ok || r.pos != size - 4) { error = "invalid exploration payload"; return false; }
  for (const auto &n : candidate.nodes_) for (const auto &l : n.links) if (distance (n.foot, candidate.nodes_[l.to].foot) > 128) { error = "invalid exploration edge length"; return false; }
  candidate.rebuild (); *this = std::move (candidate); dirty_ = false; return true;
}
} // namespace rbcc
