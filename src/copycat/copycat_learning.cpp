// Local online learning for AiBo Human Copycat. GPL-2.0-or-later.
#include "copycat_learning.h"
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
namespace {
uint32_t randomNext (uint32_t &seed) {
  if (!seed) seed = 0x9e3779b9u;
  seed ^= seed << 13u; seed ^= seed >> 17u; seed ^= seed << 5u; return seed;
}
void increment (uint32_t &n) { if (n < 1000000000u) ++n; }
float value (const LearningCell &c) { return c.recent * 0.7f + c.lifetime * 0.3f; }
void update (LearningCell &c, float target) {
  c.recent = std::clamp (c.recent + 0.16f * (target - c.recent), -20.0f, 20.0f);
  c.lifetime = std::clamp (c.lifetime + 0.025f * (target - c.lifetime), -20.0f, 20.0f);
  increment (c.visits);
}
void put32 (std::vector<uint8_t> &b, uint32_t x) {
  for (unsigned i = 0; i < 4; ++i) b.push_back (static_cast<uint8_t> (x >> (i * 8u)));
}
void putFloat (std::vector<uint8_t> &b, float x) {
  uint32_t n {}; static_assert (sizeof n == sizeof x, "32-bit float required");
  std::memcpy (&n, &x, sizeof n); put32 (b, n);
}
struct Reader {
  const std::vector<uint8_t> &bytes; size_t pos {}; bool ok {true};
  uint32_t number () {
    if (pos + 4 > bytes.size ()) { ok = false; return 0; }
    uint32_t n {}; for (unsigned i = 0; i < 4; ++i) n |= static_cast<uint32_t> (bytes[pos++]) << (i * 8u);
    return n;
  }
  float real () { const auto n = number (); float x {}; std::memcpy (&x, &n, sizeof x); return x; }
};
void putCell (std::vector<uint8_t> &bytes, const LearningCell &c) {
  putFloat (bytes, c.recent); putFloat (bytes, c.lifetime); put32 (bytes, c.visits); put32 (bytes, c.demonstrations);
}
bool getCell (Reader &r, LearningCell &c) {
  c.recent = r.real (); c.lifetime = r.real (); c.visits = r.number (); c.demonstrations = r.number ();
  return r.ok && std::isfinite (c.recent) && std::isfinite (c.lifetime) &&
    std::fabs (c.recent) <= 20 && std::fabs (c.lifetime) <= 20 && c.visits <= 1000000000u && c.demonstrations <= 1000000000u;
}
bool mapValid (const std::string &map) {
  return !map.empty () && map.size () <= 63 &&
    map.find_first_not_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos;
}
} // namespace

void OnlineLearning::clear () { *this = OnlineLearning {}; }
unsigned OnlineLearning::pattern (unsigned team) const {
  if (team > 1 || opponents_[team].observations < 8) return 0;
  if (opponents_[team].approach > 0.25f) return 1;
  return opponents_[team].stationary > 0.6f ? 2u : 0u;
}
uint16_t OnlineLearning::state (const Frame &f, bool adapt) const {
  unsigned n = std::min (static_cast<unsigned> (f.context.team), 1u);
  n = n * 3u + std::min (static_cast<unsigned> (f.context.phase), 2u);
  n = n * 2u + (f.health < 40 ? 1u : 0u);
  n = n * 3u + (!f.context.visible ? 0u : (f.context.enemyDistance < 450.0f ? 1u : 2u));
  n = n * 2u + (f.context.friends ? 1u : 0u);
  n = n * 2u + (f.context.objective ? 1u : 0u);
  return static_cast<uint16_t> (n * 3u + (adapt ? pattern (f.context.team) : 0u));
}
LearnedAction OnlineLearning::classify (const Frame &f) {
  if (f.forward < -70.0f && std::fabs (f.forward) > std::fabs (f.side)) return LearnedAction::Retreat;
  if (f.side < -65.0f) return LearnedAction::Left;
  if (f.side > 65.0f) return LearnedAction::Right;
  if (std::fabs (f.forward) < 35.0f && std::fabs (f.side) < 35.0f) return LearnedAction::Hold;
  return LearnedAction::Native;
}
void OnlineLearning::imitate (uint16_t s, LearnedAction action) {
  const auto a = static_cast<size_t> (action);
  if (s >= LearningStates || a >= LearningActions) return;
  auto &row = cells_[s]; uint32_t total {};
  for (const auto &c : row) total += c.demonstrations;
  if (total >= 4096u) for (auto &c : row) c.demonstrations /= 2;
  increment (row[a].demonstrations); increment (stats_.humanSamples); dirty_ = true;
}
LearnedAction OnlineLearning::choose (uint16_t s, uint32_t allowed, uint32_t &seed,
    float exploration, bool imitation, bool adaptation) const {
  allowed &= AllLearningActions;
  if (s >= LearningStates || !allowed) return LearnedAction::Native;
  size_t choices[LearningActions] {}; size_t count {};
  for (size_t a = 0; a < LearningActions; ++a) if (allowed & (1u << a)) choices[count++] = a;
  if (!std::isfinite (exploration)) exploration = 0;
  if (static_cast<float> (randomNext (seed) % 10000u) * 0.0001f < std::clamp (exploration, 0.0f, 0.3f))
    return static_cast<LearnedAction> (choices[randomNext (seed) % count]);
  uint32_t demos {}; for (const auto &c : cells_[s]) demos += c.demonstrations;
  size_t best = choices[0]; float highest = -1000;
  for (size_t i = 0; i < count; ++i) {
    const auto a = choices[i]; const auto &c = cells_[s][a];
    float score = value (c) + 0.02f / std::sqrt (1.0f + static_cast<float> (c.visits));
    if (a == 0) score += 0.025f; // retain native movement until evidence supports a change
    if (imitation && demos) score += 0.3f * static_cast<float> (c.demonstrations) / static_cast<float> (demos);
    if (adaptation && s % 3u == 1u) {
      if (a == 4) score += 0.09f;
      if (a == 1) score -= 0.06f;
    }
    if (adaptation && s % 3u == 2u && (a == 2 || a == 3)) score += 0.06f;
    if (score > highest) { highest = score; best = a; }
  }
  return static_cast<LearnedAction> (best);
}
void OnlineLearning::reinforce (uint16_t s, LearnedAction action, float reward,
    uint16_t next, bool terminal, bool selfPlay, uint32_t nextAllowed) {
  const auto a = static_cast<size_t> (action);
  if (s >= LearningStates || a >= LearningActions || !std::isfinite (reward) || (!terminal && next >= LearningStates)) return;
  float future = -20.0f; bool any = false;
  if (!terminal) for (size_t i = 0; i < LearningActions; ++i) if (nextAllowed & (1u << i)) {
    future = std::max (future, value (cells_[next][i])); any = true;
  }
  const float target = std::clamp (reward, -4.0f, 4.0f) + (terminal || !any ? 0.0f : 0.85f * future);
  update (cells_[s][a], target); increment (stats_.updates);
  if (selfPlay) increment (stats_.selfPlayUpdates);
  dirty_ = true;
}
void OnlineLearning::observeOpponent (unsigned team, float approach, bool stationary) {
  if (team > 1 || !std::isfinite (approach)) return;
  approach = std::clamp (approach, -1.0f, 1.0f);
  auto &p = opponents_[team]; const float still = stationary ? 1.0f : 0.0f;
  if (!p.observations) { p.approach = p.longApproach = approach; p.stationary = p.longStationary = still; }
  p.approach += 0.12f * (approach - p.approach); p.longApproach += 0.012f * (approach - p.longApproach);
  p.stationary += 0.12f * (still - p.stationary); p.longStationary += 0.012f * (still - p.longStationary);
  increment (p.observations);
  if (p.observations >= 32u && p.observations - p.lastChange >= 32u &&
      std::max (std::fabs (p.approach - p.longApproach), std::fabs (p.stationary - p.longStationary)) > 0.35f) {
    increment (p.changes); p.lastChange = p.observations;
  }
  dirty_ = true;
}
TeamTactic OnlineLearning::chooseTactic (unsigned team, uint32_t &seed, float exploration) {
  if (team > 1) return TeamTactic::Split;
  if (selected_[team] >= 0) return static_cast<TeamTactic> (selected_[team]);
  unsigned best = 0;
  if (!std::isfinite (exploration)) exploration = 0;
  if (static_cast<float> (randomNext (seed) % 10000u) * 0.0001f < std::clamp (exploration, 0.0f, 0.3f)) best = randomNext (seed) % 3u;
  else for (unsigned a = 1; a < 3; ++a) if (value (tactics_[team][a]) > value (tactics_[team][best]) + 0.015f) best = a;
  selected_[team] = static_cast<int> (best); return static_cast<TeamTactic> (best);
}
void OnlineLearning::finishTeam (unsigned team, float reward) {
  if (team > 1 || selected_[team] < 0 || !std::isfinite (reward)) return;
  update (tactics_[team][static_cast<size_t> (selected_[team])], std::clamp (reward, -1.0f, 1.0f));
  selected_[team] = -1; increment (stats_.teamRounds); dirty_ = true;
}
void OnlineLearning::newRound () { selected_.fill (-1); increment (stats_.rounds); dirty_ = true; }
const LearningCell &OnlineLearning::cell (uint16_t s, LearnedAction a) const {
  static const LearningCell empty {}; const auto n = static_cast<size_t> (a);
  return s < LearningStates && n < LearningActions ? cells_[s][n] : empty;
}
const LearningCell &OnlineLearning::tactic (unsigned team, TeamTactic a) const {
  static const LearningCell empty {}; const auto n = static_cast<size_t> (a);
  return team < 2 && n < 3 ? tactics_[team][n] : empty;
}
const OpponentPattern &OnlineLearning::opponent (unsigned team) const {
  static const OpponentPattern empty {}; return team < 2 ? opponents_[team] : empty;
}
bool OnlineLearning::save (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear (); if (!mapValid (map)) { error = "invalid map"; return false; }
  std::vector<uint8_t> b {'H','C','L','E','A','R','N','1'};
  put32 (b, env); put32 (b, LearningStates); put32 (b, LearningActions); put32 (b, static_cast<uint32_t> (map.size ()));
  b.insert (b.end (), map.begin (), map.end ());
  for (const auto &row : cells_) for (const auto &c : row) putCell (b, c);
  for (const auto &row : tactics_) for (const auto &c : row) putCell (b, c);
  for (const auto &p : opponents_) {
    putFloat (b, p.approach); putFloat (b, p.longApproach); putFloat (b, p.stationary); putFloat (b, p.longStationary);
    put32 (b, p.observations); put32 (b, p.changes); put32 (b, p.lastChange);
  }
  for (auto n : {stats_.updates, stats_.humanSamples, stats_.selfPlayUpdates, stats_.teamRounds, stats_.rounds}) put32 (b, n);
  put32 (b, hashBytes (b.data (), b.size ()));
  const auto temp = path + ".tmp"; auto f = std::fopen (temp.c_str (), "wb");
  if (!f) { error = "cannot open temporary learning file"; return false; }
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
  if (!ok) { std::remove (temp.c_str ()); error = "learning write/replace failed; previous file retained"; return false; }
  dirty_ = false; return true;
}
bool OnlineLearning::load (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear (); auto f = std::fopen (path.c_str (), "rb");
  if (!f) { error = "learning file not found"; return false; }
  std::vector<uint8_t> b (65536); const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool readError = std::ferror (f) != 0; std::fclose (f);
  const size_t expected = 24 + map.size () + LearningStates * LearningActions * 16 + 6 * 16 + 2 * 28 + 20 + 4;
  if (readError || !mapValid (map) || size != expected || size >= b.size () || std::memcmp (b.data (), "HCLEARN1", 8) != 0) {
    error = "invalid learning header/length"; return false;
  }
  b.resize (size); Reader r {b}; r.pos = 8;
  const auto storedEnv = r.number (), states = r.number (), actions = r.number (), mapSize = r.number ();
  if (storedEnv != env || states != LearningStates || actions != LearningActions || mapSize != map.size () ||
      std::memcmp (b.data () + 24, map.data (), map.size ()) != 0) { error = "learning map/environment mismatch"; return false; }
  Reader tail {b}; tail.pos = size - 4;
  if (tail.number () != hashBytes (b.data (), size - 4)) { error = "learning checksum mismatch"; return false; }
  r.pos += mapSize; OnlineLearning candidate;
  for (auto &row : candidate.cells_) for (auto &c : row) if (!getCell (r, c)) { error = "invalid Q cell"; return false; }
  for (auto &row : candidate.tactics_) for (auto &c : row) if (!getCell (r, c)) { error = "invalid team cell"; return false; }
  for (auto &p : candidate.opponents_) {
    p.approach = r.real (); p.longApproach = r.real (); p.stationary = r.real (); p.longStationary = r.real ();
    p.observations = r.number (); p.changes = r.number (); p.lastChange = r.number ();
    if (!std::isfinite (p.approach) || !std::isfinite (p.longApproach) || !std::isfinite (p.stationary) || !std::isfinite (p.longStationary) ||
        std::fabs (p.approach) > 1 || std::fabs (p.longApproach) > 1 || p.stationary < 0 || p.stationary > 1 ||
        p.longStationary < 0 || p.longStationary > 1 || p.observations > 1000000000u || p.changes > p.observations || p.lastChange > p.observations) {
      error = "invalid opponent model"; return false;
    }
  }
  candidate.stats_.updates = r.number (); candidate.stats_.humanSamples = r.number ();
  candidate.stats_.selfPlayUpdates = r.number (); candidate.stats_.teamRounds = r.number (); candidate.stats_.rounds = r.number ();
  for (auto n : {candidate.stats_.updates, candidate.stats_.humanSamples, candidate.stats_.selfPlayUpdates, candidate.stats_.teamRounds, candidate.stats_.rounds})
    if (n > 1000000000u) { error = "invalid learning counter"; return false; }
  if (!r.ok || r.pos != size - 4 || candidate.stats_.selfPlayUpdates > candidate.stats_.updates) { error = "invalid learning payload"; return false; }
  *this = candidate; dirty_ = false; return true;
}
} // namespace rbcc
