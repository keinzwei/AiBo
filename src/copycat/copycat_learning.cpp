#if defined(YSTL_TESTS)
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
#else
// Local online learning for AiBo Human Copycat (Level 5 Apex: Self-Modifying Hebbian Plasticity, Structural Neuroevolution, Liquid Recurrent Core, Latent World Model & RND Curiosity). GPL-2.0-or-later.
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
float leakyRelu (float x) { return x >= 0.0f ? x : 0.15f * x; }
float leakyReluDeriv (float pre) { return pre >= 0.0f ? 1.0f : 0.15f; }
std::array<float, MlpHiddenDim> computeHidden (const TinyMlpPolicy &mlp,
    const std::array<float, ContinuousFeatureDim> &feat,
    std::array<float, MlpHiddenDim> *preOut = nullptr) {
  std::array<float, MlpHiddenDim> h {};
  for (size_t i = 0; i < MlpHiddenDim; ++i) {
    float sum = mlp.b1[i];
    const auto &row = mlp.w1[i];
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) sum += row[j] * feat[j];
    sum = std::clamp (sum, -12.0f, 12.0f);
    if (preOut) (*preOut)[i] = sum;
    h[i] = leakyRelu (sum);
  }
  return h;
}
void trainMlpStep (TinyMlpPolicy &mlp, const std::array<float, ContinuousFeatureDim> &feat,
    size_t action, float targetValue, float advantage, float lrActor, float lrCritic,
    bool imitationTarget = false, float targetStrafe = 0.65f, float targetFwd = 0.0f, float targetCrouch = 0.0f) {
  if (action >= LearningActions || !std::isfinite (targetValue) || !std::isfinite (advantage)) return;
  for (float x : feat) if (!std::isfinite (x)) return;
  std::array<float, MlpHiddenDim> pre {};
  const auto h = computeHidden (mlp, feat, &pre);

  float vPred = mlp.vb;
  for (size_t i = 0; i < MlpHiddenDim; ++i) vPred += mlp.vw[i] * h[i];
  vPred = std::clamp (vPred, -20.0f, 20.0f);
  const float tdErr = std::clamp (targetValue - vPred, -4.0f, 4.0f);

  std::array<float, LearningActions> logits {};
  float maxLogit = -1e30f;
  for (size_t a = 0; a < LearningActions; ++a) {
    float sum = mlp.b2[a];
    for (size_t i = 0; i < MlpHiddenDim; ++i) sum += mlp.w2[a][i] * h[i];
    logits[a] = std::clamp (sum, -15.0f, 15.0f);
    if (logits[a] > maxLogit) maxLogit = logits[a];
  }
  std::array<float, LearningActions> probs {};
  float denom = 0.0f;
  for (size_t a = 0; a < LearningActions; ++a) {
    probs[a] = std::exp (logits[a] - maxLogit);
    denom += probs[a];
  }
  if (denom <= 1e-9f) denom = 1.0f;
  for (size_t a = 0; a < LearningActions; ++a) probs[a] /= denom;

  const float signal = std::clamp (imitationTarget ? 1.0f : advantage, -3.5f, 3.5f);
  std::array<float, MlpHiddenDim> gradH {};

  for (size_t a = 0; a < LearningActions; ++a) {
    const float indicator = (a == action) ? 1.0f : 0.0f;
    const float entropyGrad = -0.008f * (probs[a] - (1.0f / static_cast<float> (LearningActions)));
    const float gLogit = std::clamp ((indicator - probs[a]) * signal + entropyGrad, -1.5f, 1.5f);
    mlp.b2[a] = std::clamp (mlp.b2[a] + lrActor * gLogit, -8.0f, 8.0f);
    for (size_t i = 0; i < MlpHiddenDim; ++i) {
      gradH[i] += mlp.w2[a][i] * gLogit * lrActor;
      mlp.w2[a][i] = std::clamp (mlp.w2[a][i] + lrActor * gLogit * h[i] - 0.0001f * mlp.w2[a][i], -4.0f, 4.0f);
    }
  }

  if (lrCritic > 0.0f) {
    mlp.vb = std::clamp (mlp.vb + lrCritic * tdErr, -10.0f, 10.0f);
    for (size_t i = 0; i < MlpHiddenDim; ++i) {
      gradH[i] += mlp.vw[i] * tdErr * lrCritic;
      mlp.vw[i] = std::clamp (mlp.vw[i] + lrCritic * tdErr * h[i] - 0.0001f * mlp.vw[i], -4.0f, 4.0f);
    }
    mlp.avgTdError += 0.05f * (std::fabs (tdErr) - mlp.avgTdError);
  }

  if (imitationTarget || signal > 0.05f) {
    const float modLr = lrActor * 0.6f;
    const float desired0 = std::clamp ((targetStrafe - 0.70f) / 0.30f, -1.0f, 1.0f);
    const float desired1 = std::clamp (targetFwd / 0.45f, -1.0f, 1.0f);
    const float desired2 = std::clamp ((targetCrouch - 0.325f) / 0.325f, -1.0f, 1.0f);
    const float targets[3] {desired0, desired1, desired2};
    for (size_t m = 0; m < 3; ++m) {
      float sum = mlp.mb[m];
      for (size_t i = 0; i < MlpHiddenDim; ++i) sum += mlp.mw[m][i] * h[i];
      const float pred = std::tanh (std::clamp (sum, -8.0f, 8.0f));
      const float err = (targets[m] - pred) * (1.0f - pred * pred);
      mlp.mb[m] = std::clamp (mlp.mb[m] + modLr * err, -4.0f, 4.0f);
      for (size_t i = 0; i < MlpHiddenDim; ++i) {
        gradH[i] += mlp.mw[m][i] * err * modLr;
        mlp.mw[m][i] = std::clamp (mlp.mw[m][i] + modLr * err * h[i], -3.0f, 3.0f);
      }
    }
  }

  for (size_t i = 0; i < MlpHiddenDim; ++i) {
    const float gPre = std::clamp (gradH[i] * leakyReluDeriv (pre[i]), -1.0f, 1.0f);
    mlp.b1[i] = std::clamp (mlp.b1[i] + gPre, -6.0f, 6.0f);
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      mlp.w1[i][j] = std::clamp (mlp.w1[i][j] + gPre * feat[j] - 0.00005f * mlp.w1[i][j], -4.0f, 4.0f);
    }
  }
  increment (mlp.steps);
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

const char *subgoalOptionName (SubgoalOption opt) {
  static const char *names[] = {"hold_angle", "jiggle_bait", "wide_swing", "flank_rotate", "trade_support", "fallback_cover"};
  const auto i = static_cast<size_t> (opt);
  return i < SubgoalOptionCount ? names[i] : "unknown";
}

void OnlineLearning::initMlp () {
  mlp_ = TinyMlpPolicy {};
  uint32_t seed = 0xA1B04410u;
  const float scale1 = 1.0f / std::sqrt (static_cast<float> (ContinuousFeatureDim));
  const float scale2 = 0.25f / std::sqrt (static_cast<float> (MlpHiddenDim));
  for (size_t i = 0; i < MlpHiddenDim; ++i) {
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
      mlp_.w1[i][j] = r * scale1;
    }
    mlp_.b1[i] = 0.02f;
    for (size_t a = 0; a < LearningActions; ++a) {
      const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
      mlp_.w2[a][i] = r * scale2;
    }
    for (size_t m = 0; m < 3; ++m) {
      const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
      mlp_.mw[m][i] = r * scale2;
    }
    const float rv = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
    mlp_.vw[i] = rv * scale2;
  }
}

void OnlineLearning::initHrl () {
  hrl_ = HrlOptionPolicy {};
  auto &w = hrl_.wOption;
  w[0][16] = 0.85f; w[0][8] = 0.45f; w[0][19] = 0.55f; w[0][5] = 0.25f;
  w[1][20] = 0.65f; w[1][22] = 0.55f; w[1][8] = 0.35f; w[1][2] = 0.20f;
  w[2][15] = 0.55f; w[2][2] = 0.50f; w[2][7] = 0.45f; w[2][12] = 0.25f;
  w[3][6] = -0.45f; w[3][20] = 0.50f; w[3][2] = 0.30f;
  w[4][12] = 0.90f; w[4][6] = 0.35f; w[4][15] = 0.30f;
  w[5][2] = -0.95f; w[5][19] = 0.45f; w[5][18] = 0.35f;
  hrl_.bOption = {0.05f, 0.08f, 0.10f, 0.04f, 0.06f, 0.15f};
}

void OnlineLearning::initWorldModel () {
  world_ = LatentWorldModel {};
  uint32_t seed = 0x4C564C34u; // "LVL4"
  const float encScale = 0.8f / std::sqrt (static_cast<float> (ContinuousFeatureDim));
  for (size_t i = 0; i < LatentDim; ++i) {
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
      world_.wEnc[i][j] = r * encScale;
    }
    if (i < ContinuousFeatureDim) world_.wEnc[i][i] += 0.55f;
    world_.wVal[i] = (i == 2) ? 0.65f : ((i == 6) ? -0.15f : 0.08f);
  }
  const float dynScale = 0.18f / std::sqrt (static_cast<float> (LatentDim));
  for (size_t a = 0; a < LearningActions; ++a) {
    for (size_t i = 0; i < LatentDim; ++i) {
      for (size_t j = 0; j < LatentDim; ++j) {
        const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
        world_.wDyn[a][i][j] = r * dynScale;
      }
      world_.wRew[a][i] = 0.02f;
    }
    if (a == 2 || a == 3) {
      world_.bDyn[a][3] = 0.22f;
      world_.wRew[a][2] = 0.18f;
      world_.wRew[a][6] = 0.12f;
    }
    if (a == 4) {
      world_.bDyn[a][6] = -0.28f;
      world_.wRew[a][2] = -0.25f;
    }
    if (a == 1) {
      world_.bDyn[a][3] = -0.20f;
      world_.wRew[a][8] = 0.16f;
    }
  }
}

void OnlineLearning::initCuriosity () {
  curiosity_ = IntrinsicCuriosityModule {};
  uint32_t seedTarget = 0x524E4454u;
  uint32_t seedPred = 0x524E4450u;
  const float scale = 1.0f / std::sqrt (static_cast<float> (ContinuousFeatureDim));
  for (size_t i = 0; i < CuriosityEmbedDim; ++i) {
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      const float rt = static_cast<float> (static_cast<int32_t> (randomNext (seedTarget) & 65535u) - 32768) / 32768.0f;
      const float rp = static_cast<float> (static_cast<int32_t> (randomNext (seedPred) & 65535u) - 32768) / 32768.0f;
      curiosity_.wTarget[i][j] = rt * scale;
      curiosity_.wPred[i][j] = rp * scale * 0.25f;
    }
    curiosity_.bTarget[i] = static_cast<float> (static_cast<int32_t> (randomNext (seedTarget) & 1023u) - 512) * 0.0005f;
    curiosity_.bPred[i] = 0.0f;
  }
}

void OnlineLearning::initLiquidAndHebbian () {
  liquid_ = LiquidRecurrentCore {};
  hebbian_ = HebbianPlasticCortex {};
  uint32_t seed = 0x4C564C35u; // "LVL5"
  const float inScale = 0.65f / std::sqrt (static_cast<float> (ContinuousFeatureDim));
  const float recScale = 0.35f / std::sqrt (static_cast<float> (LiquidStateDim));
  uint32_t activeCount = 0;
  for (size_t i = 0; i < LiquidStateDim; ++i) {
    // Multi-timescale liquid constants: fast reflex neurons (0.08s) to slow tactical working memory (1.40s)
    liquid_.tauBase[i] = 0.08f + (static_cast<float> (i) / static_cast<float> (LiquidStateDim - 1)) * 1.32f;
    liquid_.bRec[i] = 0.01f;
    hebbian_.coeffA[i] = 0.42f;   // Oja/Hebbian correlation term (pre * post)
    hebbian_.coeffB[i] = 0.06f;   // Pre-synaptic eligibility term
    hebbian_.coeffC[i] = -0.05f;  // Homeostatic post-synaptic decay term
    hebbian_.coeffD[i] = 0.0f;    // Baseline drift
    hebbian_.eta[i] = 0.085f;     // Local plasticity rate
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
      liquid_.wIn[i][j] = r * inScale;
      hebbian_.synapseUtility[i][j] = std::fabs (liquid_.wIn[i][j]) + 0.05f;
      // Sparse initial cortical connectivity (~75% active synapses; 25% reserved for structural neurogenesis)
      const bool active = ((i + j) % 4u != 0u);
      hebbian_.activeMask[i][j] = active ? 1u : 0u;
      if (active) ++activeCount;
    }
    for (size_t k = 0; k < LiquidStateDim; ++k) {
      const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
      liquid_.wRec[i][k] = (i == k) ? 0.25f : (r * recScale);
    }
    for (size_t a = 0; a < LearningActions; ++a) {
      const float r = static_cast<float> (static_cast<int32_t> (randomNext (seed) & 65535u) - 32768) / 32768.0f;
      liquid_.wReadout[a][i] = r * 0.22f;
    }
  }
  hebbian_.activeSynapses = activeCount;
}

std::array<float, LearningActions> OnlineLearning::stepLiquidAndHebbian (
    const std::array<float, ContinuousFeatureDim> &features, float dt, float neuromodulatorDopamine,
    float *outResonance, float *outHebbianDrive) {
  std::array<float, LearningActions> readout {};
  for (float x : features) if (!std::isfinite (x)) return readout;
  dt = std::clamp (std::isfinite (dt) ? dt : 0.01f, 0.001f, 0.10f);
  const float modSignal = std::tanh (std::clamp (neuromodulatorDopamine, -3.0f, 3.0f));

  std::array<float, LiquidStateDim> nextState {};
  float resonanceSum = 0.0f;
  float hebbianMag = 0.0f;

  for (size_t i = 0; i < LiquidStateDim; ++i) {
    float drive = liquid_.bRec[i];
    // Recurrent working-memory contribution
    for (size_t k = 0; k < LiquidStateDim; ++k) {
      drive += liquid_.wRec[i][k] * liquid_.state[k];
    }
    // Self-modifying plastic + structural input synapses
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      if (!hebbian_.activeMask[i][j]) continue;
      const float effWeight = liquid_.wIn[i][j] + hebbian_.plasticTraces[i][j];
      drive += effWeight * features[j];
    }
    const float postAct = std::tanh (std::clamp (drive, -8.0f, 8.0f));
    // Input-dependent Liquid Time-Constant: faster when input drive is intense (combat), slower in calm navigation
    const float adaptiveTau = std::clamp (liquid_.tauBase[i] / (1.0f + 0.45f * std::fabs (postAct)), 0.04f, 2.0f);
    const float alpha = dt / adaptiveTau;
    nextState[i] = std::clamp ((liquid_.state[i] + alpha * postAct) / (1.0f + alpha), -1.0f, 1.0f);
    resonanceSum += std::fabs (nextState[i]);

    // Update ABCD Hebbian fast plastic traces: dH_ij = eta_i * modSignal * (A*x*y + B*x + C*y + D) - decay * H_ij
    const float eta = hebbian_.eta[i];
    const float A = hebbian_.coeffA[i], B = hebbian_.coeffB[i], C = hebbian_.coeffC[i], D = hebbian_.coeffD[i];
    const float y = nextState[i];
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      if (!hebbian_.activeMask[i][j]) continue;
      const float x = features[j];
      const float hebbDelta = eta * modSignal * (A * x * y + B * x + C * y + D);
      hebbian_.plasticTraces[i][j] = std::clamp (hebbian_.plasticTraces[i][j] * 0.985f + hebbDelta, -1.5f, 1.5f);
      const float contrib = std::fabs ((liquid_.wIn[i][j] + hebbian_.plasticTraces[i][j]) * x);
      hebbian_.synapseUtility[i][j] = 0.96f * hebbian_.synapseUtility[i][j] + 0.04f * contrib;
      hebbianMag += std::fabs (hebbian_.plasticTraces[i][j]);
    }
  }
  liquid_.state = nextState;
  increment (liquid_.ticks);
  increment (hebbian_.plasticitySteps);

  for (size_t a = 0; a < LearningActions; ++a) {
    float s = 0.0f;
    for (size_t i = 0; i < LiquidStateDim; ++i) {
      s += liquid_.wReadout[a][i] * liquid_.state[i];
    }
    readout[a] = std::clamp (s, -6.0f, 6.0f);
  }
  if (outResonance) *outResonance = resonanceSum / static_cast<float> (LiquidStateDim);
  if (outHebbianDrive) *outHebbianDrive = hebbianMag / static_cast<float> ( std::max (1u, hebbian_.activeSynapses));
  dirty_ = true;
  return readout;
}

void OnlineLearning::rewireSynapticTopology (float surpriseSignal, uint32_t &rngSeed) {
  if (!std::isfinite (surpriseSignal)) return;
  // Consolidate strong plastic traces into permanent structural weights, prune weakest active synapses, and sprout new ones
  size_t weakestI = 0, weakestJ = 0;
  float lowestUtil = 1e30f;
  bool foundActive = false;

  for (size_t i = 0; i < LiquidStateDim; ++i) {
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      if (hebbian_.activeMask[i][j]) {
        // Consolidate part of long-lived plastic trace into permanent base weight
        liquid_.wIn[i][j] = std::clamp (liquid_.wIn[i][j] + 0.10f * hebbian_.plasticTraces[i][j], -3.0f, 3.0f);
        if (hebbian_.synapseUtility[i][j] < lowestUtil) {
          lowestUtil = hebbian_.synapseUtility[i][j];
          weakestI = i;
          weakestJ = j;
          foundActive = true;
        }
      }
    }
  }
  // Synaptic Pruning: sever the most dormant connection if we have enough active synapses
  if (foundActive && hebbian_.activeSynapses > (LiquidStateDim * ContinuousFeatureDim) / 2u) {
    hebbian_.activeMask[weakestI][weakestJ] = 0u;
    hebbian_.plasticTraces[weakestI][weakestJ] = 0.0f;
    --hebbian_.activeSynapses;
    increment (hebbian_.prunedSynapses);
  }
  // Autonomous Neurogenesis: sprout a new connection in a dormant slot proportional to surprise
  const unsigned sproutCount = surpriseSignal > 0.25f ? 2u : 1u;
  for (unsigned s = 0; s < sproutCount; ++s) {
    for (unsigned attempt = 0; attempt < 24; ++attempt) {
      const size_t candI = randomNext (rngSeed) % LiquidStateDim;
      const size_t candJ = randomNext (rngSeed) % ContinuousFeatureDim;
      if (!hebbian_.activeMask[candI][candJ]) {
        hebbian_.activeMask[candI][candJ] = 1u;
        const float initW = (static_cast<float> (static_cast<int32_t> (randomNext (rngSeed) & 2047u) - 1024) / 1024.0f) * 0.20f;
        liquid_.wIn[candI][candJ] = initW;
        hebbian_.plasticTraces[candI][candJ] = 0.0f;
        hebbian_.synapseUtility[candI][candJ] = 0.12f;
        ++hebbian_.activeSynapses;
        increment (hebbian_.sproutedSynapses);
        break;
      }
    }
  }
  increment (hebbian_.topologyRewires);
  dirty_ = true;
}

void OnlineLearning::applyEvolutionaryDNA (const std::array<float, 5> &abcdEtaGenes, float tauScale) {
  tauScale = std::clamp (tauScale, 0.50f, 1.80f);
  for (size_t i = 0; i < LiquidStateDim; ++i) {
    hebbian_.coeffA[i] = std::clamp (abcdEtaGenes[0], -1.5f, 1.5f);
    hebbian_.coeffB[i] = std::clamp (abcdEtaGenes[1], -1.0f, 1.0f);
    hebbian_.coeffC[i] = std::clamp (abcdEtaGenes[2], -1.0f, 1.0f);
    hebbian_.coeffD[i] = std::clamp (abcdEtaGenes[3], -0.5f, 0.5f);
    hebbian_.eta[i] = std::clamp (abcdEtaGenes[4], 0.005f, 0.35f);
    liquid_.tauBase[i] = std::clamp ((0.08f + (static_cast<float> (i) / static_cast<float> (LiquidStateDim - 1)) * 1.32f) * tauScale, 0.04f, 2.2f);
  }
  dirty_ = true;
}

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
  unsigned pat = adapt ? pattern (f.context.team) : 0u;
  if (adapt && f.context.targetToken) {
    if (const auto *prof = playerProfile (f.context.targetToken); prof && prof->sightings >= 4) {
      if (prof->aggression > 0.22f) pat = 1u;
      else if (prof->stationary > 0.58f) pat = 2u;
    }
  }
  return static_cast<uint16_t> (n * 3u + pat);
}

std::array<float, ContinuousFeatureDim> OnlineLearning::extractFeatures (const Frame &f, bool adapt) const {
  std::array<float, ContinuousFeatureDim> x {};
  const unsigned team = std::min (static_cast<unsigned> (f.context.team), 1u);
  x[0] = team == 0 ? -1.0f : 1.0f;
  x[1] = std::clamp (static_cast<float> (f.context.phase) * 0.5f, 0.0f, 1.0f);
  x[2] = std::clamp (static_cast<float> (f.health) / 100.0f, 0.0f, 1.0f);
  const float maxSpd = std::max (f.maxSpeed, 100.0f);
  x[3] = std::clamp (f.velocity.length () / maxSpd, 0.0f, 1.2f);
  x[4] = (f.state & Ground) ? 1.0f : 0.0f;
  x[5] = (f.state & Crouched) ? 1.0f : 0.0f;
  x[6] = f.context.visible ? 1.0f : 0.0f;
  if (f.context.visible) {
    const float dist = std::clamp (f.context.enemyDistance, 0.0f, 4096.0f);
    x[7] = std::exp (-dist / 420.0f);
    x[8] = std::clamp (dist / 1800.0f, 0.0f, 1.0f);
    const float yawRad = f.context.enemyYaw * 0.01745329252f;
    x[9] = std::sin (yawRad);
    x[10] = std::cos (yawRad);
    x[11] = std::clamp (f.context.enemyPitch / 45.0f, -1.0f, 1.0f);
  }
  x[12] = std::clamp (static_cast<float> (f.context.friends) / 4.0f, 0.0f, 1.0f);
  x[13] = (f.context.objective & 1u) ? 1.0f : 0.0f;
  x[14] = (f.context.objective & 6u) ? 1.0f : 0.0f;
  x[15] = (f.weapon == 28 || f.weapon == 22 || f.weapon == 8 || f.weapon == 27 || f.weapon == 14 || f.weapon == 15) ? 1.0f : 0.0f;
  x[16] = (f.weapon == 18 || f.weapon == 3) ? 1.0f : 0.0f;
  x[17] = (f.weapon == 26 || f.weapon == 16 || f.weapon == 17) ? 1.0f : 0.0f;
  x[18] = std::clamp (f.punch.length () / 5.0f, 0.0f, 1.0f);
  if (adapt) {
    const auto *prof = f.context.targetToken ? playerProfile (f.context.targetToken) : nullptr;
    if (prof && prof->sightings >= 2) {
      x[19] = std::clamp (prof->aggression, -1.0f, 1.0f);
      x[20] = std::clamp (prof->stationary, 0.0f, 1.0f);
      x[21] = std::clamp (prof->strafeRate, 0.0f, 1.0f);
      x[22] = std::clamp (prof->duelWinRate * 0.6f + prof->awpPreference * 0.4f, 0.0f, 1.0f);
    }
    else {
      const auto &op = opponents_[team];
      x[19] = std::clamp (op.approach, -1.0f, 1.0f);
      x[20] = std::clamp (op.stationary, 0.0f, 1.0f);
      x[21] = std::clamp (1.0f - op.stationary, 0.0f, 1.0f);
      x[22] = 0.5f;
    }
  }
  x[23] = 1.0f;
  return x;
}

std::array<float, ContinuousFeatureDim> OnlineLearning::featuresFromDiscreteState (uint16_t s) const {
  std::array<float, ContinuousFeatureDim> x {};
  if (s >= LearningStates) { x[23] = 1.0f; return x; }
  unsigned rem = s;
  const unsigned pat = rem % 3u; rem /= 3u;
  const unsigned obj = rem % 2u; rem /= 2u;
  const unsigned frd = rem % 2u; rem /= 2u;
  const unsigned vis = rem % 3u; rem /= 3u;
  const unsigned lowHp = rem % 2u; rem /= 2u;
  const unsigned phase = rem % 3u; rem /= 3u;
  const unsigned team = rem % 2u;

  x[0] = team == 0 ? -1.0f : 1.0f;
  x[1] = static_cast<float> (phase) * 0.5f;
  x[2] = lowHp ? 0.25f : 0.85f;
  x[3] = 0.5f;
  x[4] = 1.0f;
  x[5] = 0.0f;
  x[6] = vis > 0 ? 1.0f : 0.0f;
  x[7] = vis == 1 ? 0.75f : (vis == 2 ? 0.25f : 0.0f);
  x[8] = vis == 1 ? 0.18f : (vis == 2 ? 0.55f : 0.0f);
  x[10] = vis > 0 ? 1.0f : 0.0f;
  x[12] = frd ? 0.5f : 0.0f;
  x[13] = obj ? 1.0f : 0.0f;
  x[15] = 1.0f;
  x[19] = pat == 1 ? 0.5f : (pat == 2 ? -0.2f : 0.0f);
  x[20] = pat == 2 ? 0.8f : 0.2f;
  x[21] = pat == 2 ? 0.2f : 0.6f;
  x[22] = 0.5f;
  x[23] = 1.0f;
  return x;
}

float OnlineLearning::evaluateValue (const std::array<float, ContinuousFeatureDim> &features) const {
  const auto h = computeHidden (mlp_, features);
  float v = mlp_.vb;
  for (size_t i = 0; i < MlpHiddenDim; ++i) v += mlp_.vw[i] * h[i];
  return std::clamp (v, -20.0f, 20.0f);
}

std::array<float, LearningActions> OnlineLearning::evaluatePolicy (
    const std::array<float, ContinuousFeatureDim> &features, ContinuousModulator *mod) const {
  const auto h = computeHidden (mlp_, features);
  std::array<float, LearningActions> scores {};
  for (size_t a = 0; a < LearningActions; ++a) {
    float sum = mlp_.b2[a];
    for (size_t i = 0; i < MlpHiddenDim; ++i) sum += mlp_.w2[a][i] * h[i];
    scores[a] = std::clamp (sum, -15.0f, 15.0f);
  }
  if (mod) {
    float m0 = mlp_.mb[0], m1 = mlp_.mb[1], m2 = mlp_.mb[2];
    for (size_t i = 0; i < MlpHiddenDim; ++i) {
      m0 += mlp_.mw[0][i] * h[i];
      m1 += mlp_.mw[1][i] * h[i];
      m2 += mlp_.mw[2][i] * h[i];
    }
    mod->strafeScale = std::clamp (0.70f + 0.30f * std::tanh (m0), 0.40f, 1.00f);
    mod->forwardBias = std::clamp (0.45f * std::tanh (m1), -0.45f, 0.45f);
    mod->crouchLikelihood = std::clamp (0.325f * (1.0f + std::tanh (m2)), 0.0f, 0.65f);
    mod->confidence = std::clamp (static_cast<float> (mlp_.steps) / 128.0f, 0.0f, 1.0f);
  }
  return scores;
}

std::array<float, LatentDim> OnlineLearning::encodeLatent (const std::array<float, ContinuousFeatureDim> &features) const {
  std::array<float, LatentDim> z {};
  for (size_t i = 0; i < LatentDim; ++i) {
    float sum = world_.bEnc[i];
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) sum += world_.wEnc[i][j] * features[j];
    z[i] = std::tanh (std::clamp (sum, -8.0f, 8.0f));
  }
  return z;
}

std::array<float, LatentDim> OnlineLearning::predictNextLatent (const std::array<float, LatentDim> &z,
    LearnedAction action, float *predictedReward) const {
  const auto a = std::min (static_cast<size_t> (action), LearningActions - 1u);
  std::array<float, LatentDim> nextZ {};
  for (size_t i = 0; i < LatentDim; ++i) {
    float delta = world_.bDyn[a][i];
    for (size_t j = 0; j < LatentDim; ++j) delta += world_.wDyn[a][i][j] * z[j];
    nextZ[i] = std::tanh (std::clamp (z[i] + delta, -8.0f, 8.0f));
  }
  if (predictedReward) {
    float r = world_.bRew[a];
    for (size_t i = 0; i < LatentDim; ++i) r += world_.wRew[a][i] * z[i];
    *predictedReward = std::clamp (r, -4.0f, 4.0f);
  }
  return nextZ;
}

WorldModelRollout OnlineLearning::imagineRollout (const Frame &frame, uint32_t allowed, bool adapt) const {
  WorldModelRollout out {};
  allowed &= AllLearningActions;
  if (!allowed) return out;
  const auto feat = extractFeatures (frame, adapt);
  const auto z0 = encodeLatent (feat);
  constexpr float gamma = 0.85f;

  auto latentValue = [&] (const std::array<float, LatentDim> &z) -> float {
    float v = world_.bVal;
    for (size_t i = 0; i < LatentDim; ++i) v += world_.wVal[i] * z[i];
    return std::clamp (v, -10.0f, 10.0f);
  };

  float bestScore = -1e30f;
  size_t bestAct = 0;
  for (size_t a0 = 0; a0 < LearningActions; ++a0) {
    if (!(allowed & (1u << a0))) {
      out.rolloutScores[a0] = -100.0f;
      continue;
    }
    float r1 = 0.0f;
    const auto z1 = predictNextLatent (z0, static_cast<LearnedAction> (a0), &r1);
    float bestBranch = -1e30f;
    for (size_t a1 = 0; a1 < LearningActions; ++a1) {
      if (!(allowed & (1u << a1))) continue;
      float r2 = 0.0f;
      const auto z2 = predictNextLatent (z1, static_cast<LearnedAction> (a1), &r2);
      float bestStep3 = -1e30f;
      for (size_t a2 = 0; a2 < LearningActions; ++a2) {
        if (!(allowed & (1u << a2))) continue;
        float r3 = 0.0f;
        const auto z3 = predictNextLatent (z2, static_cast<LearnedAction> (a2), &r3);
        const float g3 = r3 + gamma * latentValue (z3);
        if (g3 > bestStep3) bestStep3 = g3;
      }
      const float branchReturn = r2 + gamma * (bestStep3 > -1e20f ? bestStep3 : latentValue (z2));
      if (branchReturn > bestBranch) bestBranch = branchReturn;
    }
    const float totalReturn = r1 + gamma * (bestBranch > -1e20f ? bestBranch : latentValue (z1));
    out.rolloutScores[a0] = totalReturn;
    if (totalReturn > bestScore) {
      bestScore = totalReturn;
      bestAct = a0;
    }
  }
  out.bestAction = static_cast<LearnedAction> (bestAct);
  out.expectedReturn = bestScore > -1e20f ? bestScore : 0.0f;
  out.survivalProb = std::clamp (0.5f + 0.35f * std::tanh (z0[2] + 0.25f * out.expectedReturn), 0.05f, 0.99f);
  return out;
}

void OnlineLearning::trainWorldModel (const std::array<float, ContinuousFeatureDim> &prevFeat, LearnedAction action,
    float actualReward, const std::array<float, ContinuousFeatureDim> &nextFeat, bool terminal) {
  const auto a = static_cast<size_t> (action);
  if (a >= LearningActions || !std::isfinite (actualReward)) return;
  for (float x : prevFeat) if (!std::isfinite (x)) return;
  for (float x : nextFeat) if (!std::isfinite (x)) return;

  const auto z = encodeLatent (prevFeat);
  const auto targetZ = terminal ? std::array<float, LatentDim> {} : encodeLatent (nextFeat);
  float predR = 0.0f;
  const auto predZ = predictNextLatent (z, action, &predR);

  constexpr float lrDyn = 0.028f;
  constexpr float lrRew = 0.035f;
  float lossSum = 0.0f;
  for (size_t i = 0; i < LatentDim; ++i) {
    const float errZ = std::clamp (targetZ[i] - predZ[i], -2.0f, 2.0f);
    lossSum += errZ * errZ;
    const float gZ = errZ * (1.0f - predZ[i] * predZ[i]);
    world_.bDyn[a][i] = std::clamp (world_.bDyn[a][i] + lrDyn * gZ, -3.0f, 3.0f);
    for (size_t j = 0; j < LatentDim; ++j) {
      world_.wDyn[a][i][j] = std::clamp (world_.wDyn[a][i][j] + lrDyn * gZ * z[j] - 0.0001f * world_.wDyn[a][i][j], -3.0f, 3.0f);
    }
  }
  const float errR = std::clamp (actualReward - predR, -4.0f, 4.0f);
  lossSum += errR * errR;
  world_.bRew[a] = std::clamp (world_.bRew[a] + lrRew * errR, -4.0f, 4.0f);
  for (size_t i = 0; i < LatentDim; ++i) {
    world_.wRew[a][i] = std::clamp (world_.wRew[a][i] + lrRew * errR * z[i] - 0.0001f * world_.wRew[a][i], -3.0f, 3.0f);
  }

  float nextV = 0.0f;
  if (!terminal) {
    nextV = world_.bVal;
    for (size_t i = 0; i < LatentDim; ++i) nextV += world_.wVal[i] * targetZ[i];
    nextV = std::clamp (nextV, -10.0f, 10.0f);
  }
  float curV = world_.bVal;
  for (size_t i = 0; i < LatentDim; ++i) curV += world_.wVal[i] * z[i];
  const float tdVal = std::clamp ((actualReward + 0.85f * nextV) - curV, -4.0f, 4.0f);
  world_.bVal = std::clamp (world_.bVal + 0.03f * tdVal, -8.0f, 8.0f);
  for (size_t i = 0; i < LatentDim; ++i) {
    world_.wVal[i] = std::clamp (world_.wVal[i] + 0.03f * tdVal * z[i], -4.0f, 4.0f);
  }
  world_.avgModelLoss += 0.05f * ((lossSum / static_cast<float> (LatentDim + 1)) - world_.avgModelLoss);
  increment (world_.trainSteps);
  dirty_ = true;
}

float OnlineLearning::computeCuriosity (const std::array<float, ContinuousFeatureDim> &features) const {
  float sqErr = 0.0f;
  for (size_t i = 0; i < CuriosityEmbedDim; ++i) {
    float t = curiosity_.bTarget[i];
    float p = curiosity_.bPred[i];
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      t += curiosity_.wTarget[i][j] * features[j];
      p += curiosity_.wPred[i][j] * features[j];
    }
    const float diff = std::tanh (std::clamp (p, -8.0f, 8.0f)) - std::tanh (std::clamp (t, -8.0f, 8.0f));
    sqErr += diff * diff;
  }
  return std::clamp (sqErr / static_cast<float> (CuriosityEmbedDim), 0.0f, 1.5f);
}

float OnlineLearning::observeAndTrainCuriosity (const std::array<float, ContinuousFeatureDim> &features) {
  for (float x : features) if (!std::isfinite (x)) return 0.0f;
  float sqErr = 0.0f;
  constexpr float lrPred = 0.045f;
  for (size_t i = 0; i < CuriosityEmbedDim; ++i) {
    float tSum = curiosity_.bTarget[i];
    float pSum = curiosity_.bPred[i];
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      tSum += curiosity_.wTarget[i][j] * features[j];
      pSum += curiosity_.wPred[i][j] * features[j];
    }
    const float targetOut = std::tanh (std::clamp (tSum, -8.0f, 8.0f));
    const float predOut = std::tanh (std::clamp (pSum, -8.0f, 8.0f));
    const float err = targetOut - predOut;
    sqErr += err * err;
    const float grad = err * (1.0f - predOut * predOut);
    curiosity_.bPred[i] = std::clamp (curiosity_.bPred[i] + lrPred * grad, -6.0f, 6.0f);
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      curiosity_.wPred[i][j] = std::clamp (curiosity_.wPred[i][j] + lrPred * grad * features[j], -4.0f, 4.0f);
    }
  }
  const float bonus = std::clamp (sqErr / static_cast<float> (CuriosityEmbedDim), 0.0f, 1.5f);
  curiosity_.avgCuriosity += 0.05f * (bonus - curiosity_.avgCuriosity);
  increment (curiosity_.steps);
  dirty_ = true;
  return bonus;
}

bool OnlineLearning::shouldTerminateOption (const Frame &frame, SubgoalOption currentOption,
    float optionElapsedSec, bool adaptation) const {
  if (optionElapsedSec >= 4.2f) return true;
  if (optionElapsedSec < 0.45f) return false;
  if (frame.health < 35 && currentOption != SubgoalOption::FallBackCover) return true;
  if (frame.context.visible && currentOption == SubgoalOption::FlankRotate) return true;
  const auto feat = extractFeatures (frame, adaptation);
  const auto curIdx = static_cast<size_t> (currentOption);
  if (curIdx >= SubgoalOptionCount) return true;
  float curScore = hrl_.bOption[curIdx] + hrl_.optionValue[curIdx] * 0.5f;
  for (size_t j = 0; j < ContinuousFeatureDim; ++j) curScore += hrl_.wOption[curIdx][j] * feat[j];
  for (size_t o = 0; o < SubgoalOptionCount; ++o) {
    if (o == curIdx) continue;
    float s = hrl_.bOption[o] + hrl_.optionValue[o] * 0.5f;
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) s += hrl_.wOption[o][j] * feat[j];
    if (s > curScore + 0.38f) return true;
  }
  return false;
}

SubgoalOption OnlineLearning::chooseOption (const Frame &frame, SubgoalOption currentOption,
    float optionElapsedSec, uint32_t &seed, float exploration, bool adaptation) {
  if (!shouldTerminateOption (frame, currentOption, optionElapsedSec, adaptation))
    return currentOption;
  increment (hrl_.switches);
  if (!std::isfinite (exploration)) exploration = 0.0f;
  if (static_cast<float> (randomNext (seed) % 10000u) * 0.0001f < std::clamp (exploration, 0.0f, 0.3f))
    return static_cast<SubgoalOption> (randomNext (seed) % SubgoalOptionCount);
  const auto feat = extractFeatures (frame, adaptation);
  size_t best = 0;
  float highest = -1e30f;
  for (size_t o = 0; o < SubgoalOptionCount; ++o) {
    float s = hrl_.bOption[o] + hrl_.optionValue[o] * 0.6f;
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) s += hrl_.wOption[o][j] * feat[j];
    s += static_cast<float> (randomNext (seed) & 63u) * 0.0015f;
    if (s > highest) { highest = s; best = o; }
  }
  dirty_ = true;
  return static_cast<SubgoalOption> (best);
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
  increment (row[a].demonstrations); increment (stats_.humanSamples);
  trainMlpStep (mlp_, featuresFromDiscreteState (s), a, 0.25f, 0.45f, 0.035f, 0.0f, true);
  dirty_ = true;
}

void OnlineLearning::imitateContinuous (const Frame &frame, LearnedAction action, bool adapt) {
  const auto s = state (frame, adapt);
  const auto a = static_cast<size_t> (action);
  if (s >= LearningStates || a >= LearningActions) return;
  auto &row = cells_[s]; uint32_t total {};
  for (const auto &c : row) total += c.demonstrations;
  if (total >= 4096u) for (auto &c : row) c.demonstrations /= 2;
  increment (row[a].demonstrations); increment (stats_.humanSamples);
  const float maxSpd = std::max (frame.maxSpeed, 100.0f);
  const float strafeScale = std::clamp (std::fabs (frame.side) / maxSpd, 0.45f, 1.0f);
  const float fwdBias = std::clamp (frame.forward / maxSpd, -0.45f, 0.45f);
  const float crouch = (frame.buttons & Duck) ? 0.55f : 0.05f;
  trainMlpStep (mlp_, extractFeatures (frame, adapt), a, 0.35f, 0.55f, 0.045f, 0.0f, true, strafeScale, fwdBias, crouch);
  dirty_ = true;
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
  uint32_t demos {}, totalVisits {};
  for (const auto &c : cells_[s]) { demos += c.demonstrations; totalVisits += c.visits; }
  const auto mlpScores = evaluatePolicy (featuresFromDiscreteState (s));
  const float mlpWeight = mlp_.steps > 0 ? std::clamp (0.18f / (1.0f + static_cast<float> (totalVisits) * 0.25f), 0.02f, 0.18f) : 0.0f;
  const float nativeBias = 0.025f / (1.0f + static_cast<float> (totalVisits) * 0.1f);
  size_t best = choices[0]; float highest = -1000;
  for (size_t i = 0; i < count; ++i) {
    const auto a = choices[i]; const auto &c = cells_[s][a];
    float score = value (c) + mlpWeight * mlpScores[a] + 0.02f / std::sqrt (1.0f + static_cast<float> (c.visits));
    if (a == 0) score += nativeBias;
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

ContinuousModulator OnlineLearning::chooseContinuous (const Frame &frame, uint32_t allowed, uint32_t &seed,
    float exploration, bool imitation, bool adaptation, SubgoalOption activeOption) {
  ContinuousModulator mod {};
  mod.option = activeOption;
  allowed &= AllLearningActions;
  const auto s = state (frame, adaptation);
  if (s >= LearningStates || !allowed) return mod;
  const auto feat = extractFeatures (frame, adaptation);
  const auto mlpScores = evaluatePolicy (feat, &mod);
  mod.option = activeOption;

  // Level 4: 3-step Mental Rollout Imagination + RND Novelty evaluation
  const auto rollout = imagineRollout (frame, allowed, adaptation);
  increment (world_.rollouts);
  mod.expectedReturn = rollout.expectedReturn;
  mod.noveltyBonus = computeCuriosity (feat);

  // Level 5: Step Liquid Time-Constant Recurrent Working Memory & Self-Modifying ABCD Hebbian Synapses
  const float dt = std::clamp (static_cast<float> (frame.msec > 0 ? frame.msec : 16) * 0.001f, 0.005f, 0.05f);
  const float dopamineDrive = 0.35f * mod.expectedReturn + 0.45f * mod.noveltyBonus;
  const auto liquidScores = stepLiquidAndHebbian (feat, dt, dopamineDrive, &mod.liquidResonance, &mod.hebbianModulation);

  size_t choices[LearningActions] {}; size_t count {};
  for (size_t a = 0; a < LearningActions; ++a) if (allowed & (1u << a)) choices[count++] = a;
  if (!std::isfinite (exploration)) exploration = 0;
  const float curiosityBoost = std::clamp (mod.noveltyBonus * 0.04f, 0.0f, 0.05f);
  if (static_cast<float> (randomNext (seed) % 10000u) * 0.0001f < std::clamp (exploration + curiosityBoost, 0.0f, 0.32f)) {
    mod.action = static_cast<LearnedAction> (choices[randomNext (seed) % count]);
    return mod;
  }

  uint32_t demos {}, totalVisits {};
  for (const auto &c : cells_[s]) { demos += c.demonstrations; totalVisits += c.visits; }
  const float mlpBlend = std::clamp (0.28f / (1.0f + static_cast<float> (totalVisits) * 0.15f) + 0.08f * mod.confidence, 0.05f, 0.36f);
  const float worldBlend = std::clamp (0.22f / (1.0f + static_cast<float> (totalVisits) * 0.20f) + (world_.trainSteps > 8 ? 0.12f : 0.04f), 0.04f, 0.30f);
  const float liquidBlend = std::clamp (0.12f / (1.0f + static_cast<float> (totalVisits) * 0.25f) + 0.05f * mod.liquidResonance, 0.02f, 0.18f);
  const float nativeBias = 0.025f / (1.0f + static_cast<float> (totalVisits + mlp_.steps / 8u) * 0.12f);

  const auto *prof = (adaptation && frame.context.targetToken) ? playerProfile (frame.context.targetToken) : nullptr;
  size_t best = choices[0]; float highest = -1000.0f;
  for (size_t i = 0; i < count; ++i) {
    const auto a = choices[i]; const auto &c = cells_[s][a];
    float score = value (c) + mlpBlend * mlpScores[a] + worldBlend * rollout.rolloutScores[a] +
                  liquidBlend * liquidScores[a] + 0.025f / std::sqrt (1.0f + static_cast<float> (c.visits));
    if (a == 0) score += nativeBias;
    if (imitation && demos) score += 0.3f * static_cast<float> (c.demonstrations) / static_cast<float> (demos);
    if (adaptation) {
      if (s % 3u == 1u) {
        if (a == 4) score += 0.09f;
        if (a == 1) score -= 0.06f;
      }
      if (s % 3u == 2u && (a == 2 || a == 3)) score += 0.06f;
      if (prof && prof->sightings >= 3) {
        if (prof->awpPreference > 0.5f && (a == 2 || a == 3)) score += 0.08f;
        if (prof->awpPreference > 0.5f && a == 1) score -= 0.10f;
        if (prof->aggression > 0.4f && a == 4) score += 0.07f;
      }
      const float hrlScale = 1.0f / (1.0f + static_cast<float> (totalVisits) * 0.25f);
      switch (activeOption) {
      case SubgoalOption::HoldAngle:
        if (a == 1) score += 0.045f * hrlScale;
        break;
      case SubgoalOption::JiggleBait:
      case SubgoalOption::WideSwing:
        if (a == 2 || a == 3) score += 0.045f * hrlScale;
        break;
      case SubgoalOption::FallBackCover:
        if (a == 4) score += 0.065f * hrlScale;
        break;
      case SubgoalOption::FlankRotate:
      case SubgoalOption::TradeSupport:
      default:
        if (a == 0 || a == 2 || a == 3) score += 0.025f * hrlScale;
        break;
      }
    }
    if (score > highest) { highest = score; best = a; }
  }
  mod.action = static_cast<LearnedAction> (best);
  if (activeOption == SubgoalOption::WideSwing) {
    mod.strafeScale = std::clamp (mod.strafeScale + 0.14f, 0.55f, 1.00f);
  }
  else if (activeOption == SubgoalOption::JiggleBait) {
    mod.strafeScale = std::clamp (mod.strafeScale, 0.48f, 0.78f);
  }
  if (prof && prof->sightings >= 3 && prof->awpPreference > 0.5f) {
    mod.strafeScale = std::clamp (mod.strafeScale + 0.12f, 0.50f, 1.00f);
    mod.crouchLikelihood = std::clamp (mod.crouchLikelihood + 0.15f, 0.0f, 0.65f);
  }
  return mod;
}

void OnlineLearning::reinforce (uint16_t s, LearnedAction action, float reward,
    uint16_t next, bool terminal, bool selfPlay, uint32_t nextAllowed) {
  const auto a = static_cast<size_t> (action);
  if (s >= LearningStates || a >= LearningActions || !std::isfinite (reward) || (!terminal && next >= LearningStates)) return;
  float future = -20.0f; bool any = false;
  if (!terminal) for (size_t i = 0; i < LearningActions; ++i) if (nextAllowed & (1u << i)) {
    future = std::max (future, value (cells_[next][i])); any = true;
  }
  const float prevVal = value (cells_[s][a]);
  const float target = std::clamp (reward, -4.0f, 4.0f) + (terminal || !any ? 0.0f : 0.85f * future);
  update (cells_[s][a], target); increment (stats_.updates);
  if (selfPlay) increment (stats_.selfPlayUpdates);
  const auto feat = featuresFromDiscreteState (s);
  const auto nextFeat = terminal ? std::array<float, ContinuousFeatureDim> {} : featuresFromDiscreteState (next);
  const float advantage = target - prevVal;
  trainMlpStep (mlp_, feat, a, target, advantage, 0.025f, 0.04f, false);
  trainWorldModel (feat, action, reward, nextFeat, terminal);
  observeAndTrainCuriosity (feat);
  stepLiquidAndHebbian (feat, 0.015f, advantage);
  dirty_ = true;
}

void OnlineLearning::reinforceContinuous (const Frame &prevFrame, LearnedAction action, float reward,
    const Frame &nextFrame, bool terminal, bool selfPlay, uint32_t nextAllowed, bool adapt, SubgoalOption activeOption) {
  const auto s = state (prevFrame, adapt);
  const auto next = state (nextFrame, adapt);
  const auto a = static_cast<size_t> (action);
  if (s >= LearningStates || a >= LearningActions || !std::isfinite (reward) || (!terminal && next >= LearningStates)) return;
  float future = -20.0f; bool any = false;
  if (!terminal) for (size_t i = 0; i < LearningActions; ++i) if (nextAllowed & (1u << i)) {
    future = std::max (future, value (cells_[next][i])); any = true;
  }
  const auto prevFeat = extractFeatures (prevFrame, adapt);
  const auto nextFeat = extractFeatures (nextFrame, adapt);
  const float intrinsicBonus = observeAndTrainCuriosity (prevFeat);
  trainWorldModel (prevFeat, action, reward, nextFeat, terminal);

  const float augmentedReward = std::clamp (reward + 0.08f * intrinsicBonus, -4.0f, 4.0f);
  const float nextCriticVal = terminal ? 0.0f : evaluateValue (nextFeat);
  const float blendedFuture = (terminal || !any) ? 0.0f : (0.65f * future + 0.35f * nextCriticVal);
  const float target = augmentedReward + 0.85f * blendedFuture;
  const float baseline = 0.5f * value (cells_[s][a]) + 0.5f * evaluateValue (prevFeat);
  const float advantage = target - baseline;
  update (cells_[s][a], target); increment (stats_.updates);
  if (selfPlay) increment (stats_.selfPlayUpdates);
  trainMlpStep (mlp_, prevFeat, a, target, advantage, 0.032f, 0.045f, false);

  // Level 5: Neuromodulated Hebbian Update + Readout weight update + Structural Synaptic Rewiring
  stepLiquidAndHebbian (prevFeat, 0.016f, advantage + 0.25f * intrinsicBonus);
  for (size_t i = 0; i < LiquidStateDim; ++i) {
    liquid_.wReadout[a][i] = std::clamp (liquid_.wReadout[a][i] + 0.028f * std::clamp (advantage, -2.5f, 2.5f) * liquid_.state[i], -3.0f, 3.0f);
  }
  if ((stats_.updates % 8u) == 0u || std::fabs (advantage) > 0.85f) {
    uint32_t rewireSeed = stats_.updates ^ static_cast<uint32_t> (s * 2654435761u);
    rewireSynapticTopology (std::fabs (advantage) + intrinsicBonus, rewireSeed);
  }

  // Level 2–5 HRL Option-Critic update
  const auto optIdx = static_cast<size_t> (activeOption);
  if (optIdx < SubgoalOptionCount) {
    const float optErr = std::clamp (target - hrl_.optionValue[optIdx], -3.0f, 3.0f);
    hrl_.optionValue[optIdx] = std::clamp (hrl_.optionValue[optIdx] + 0.12f * optErr, -8.0f, 8.0f);
    hrl_.bOption[optIdx] = std::clamp (hrl_.bOption[optIdx] + 0.025f * optErr, -4.0f, 4.0f);
    for (size_t j = 0; j < ContinuousFeatureDim; ++j) {
      hrl_.wOption[optIdx][j] = std::clamp (hrl_.wOption[optIdx][j] + 0.025f * optErr * prevFeat[j], -3.0f, 3.0f);
    }
    increment (hrl_.updates);
  }
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

void OnlineLearning::observePlayer (uint32_t token, unsigned opponentTeam, float approach, float lateralSpeed,
    bool crouched, uint8_t weapon, const Vec &pos) {
  if (!token || !std::isfinite (approach) || !std::isfinite (lateralSpeed)) return;
  approach = std::clamp (approach, -1.0f, 1.0f);
  const float latNorm = std::clamp (lateralSpeed / 250.0f, 0.0f, 1.0f);
  const float still = (std::fabs (approach) < 0.12f && latNorm < 0.14f) ? 1.0f : 0.0f;
  const float duck = crouched ? 1.0f : 0.0f;
  const float awp = (weapon == 18 || weapon == 3) ? 1.0f : 0.0f;

  OpponentPlayerProfile *slot = nullptr;
  for (auto &p : players_) if (p.token == token) { slot = &p; break; }
  if (!slot) {
    for (auto &p : players_) if (p.token == 0) { slot = &p; break; }
  }
  if (!slot) {
    size_t weakest = 0;
    for (size_t i = 1; i < MaxTrackedPlayers; ++i)
      if (players_[i].sightings < players_[weakest].sightings) weakest = i;
    players_[weakest] = OpponentPlayerProfile {};
    slot = &players_[weakest];
  }
  if (slot->sightings == 0) {
    slot->token = token;
    slot->team = opponentTeam;
    slot->aggression = approach;
    slot->stationary = still;
    slot->strafeRate = latNorm;
    slot->crouchRate = duck;
    slot->awpPreference = awp;
    slot->duelWinRate = 0.5f;
  }
  else {
    constexpr float alpha = 0.14f;
    slot->team = opponentTeam;
    slot->aggression += alpha * (approach - slot->aggression);
    slot->stationary += alpha * (still - slot->stationary);
    slot->strafeRate += alpha * (latNorm - slot->strafeRate);
    slot->crouchRate += alpha * (duck - slot->crouchRate);
    slot->awpPreference += alpha * (awp - slot->awpPreference);
  }
  slot->lastPos = pos;
  increment (slot->sightings);
  dirty_ = true;
}

void OnlineLearning::recordDuel (uint32_t token, bool botWon) {
  if (!token) return;
  for (auto &p : players_) {
    if (p.token == token) {
      const float oppWon = botWon ? 0.0f : 1.0f;
      p.duelWinRate = std::clamp (p.duelWinRate + 0.18f * (oppWon - p.duelWinRate), 0.0f, 1.0f);
      increment (p.duels);
      dirty_ = true;
      return;
    }
  }
}

const OpponentPlayerProfile *OnlineLearning::playerProfile (uint32_t token) const {
  if (!token) return nullptr;
  for (const auto &p : players_) if (p.token == token && p.sightings > 0) return &p;
  return nullptr;
}

size_t OnlineLearning::trackedPlayers () const {
  size_t count = 0;
  for (const auto &p : players_) if (p.token != 0 && p.sightings > 0) ++count;
  return count;
}

TeamTactic OnlineLearning::chooseTactic (unsigned team, uint32_t &seed, float exploration) {
  if (team > 1) return TeamTactic::Split;
  if (selected_[team] >= 0) return static_cast<TeamTactic> (selected_[team]);
  unsigned best = 0;
  if (!std::isfinite (exploration)) exploration = 0;
  if (static_cast<float> (randomNext (seed) % 10000u) * 0.0001f < std::clamp (exploration, 0.0f, 0.3f)) best = randomNext (seed) % 3u;
  else {
    float highest = -1000.0f;
    for (unsigned a = 0; a < 3; ++a) {
      const auto &c = tactics_[team][a];
      const float ucb = value (c) + 0.04f / std::sqrt (1.0f + static_cast<float> (c.visits));
      if (ucb > highest + (a == 0 ? 0.0f : 0.012f)) { highest = ucb; best = a; }
    }
  }
  selected_[team] = static_cast<int> (best); return static_cast<TeamTactic> (best);
}

void OnlineLearning::finishTeam (unsigned team, float reward) {
  if (team > 1 || selected_[team] < 0 || !std::isfinite (reward)) return;
  update (tactics_[team][static_cast<size_t> (selected_[team])], std::clamp (reward, -1.0f, 1.0f));
  selected_[team] = -1; increment (stats_.teamRounds); dirty_ = true;
}

void OnlineLearning::newRound () {
  selected_.fill (-1);
  // Decay fast Hebbian traces mildly between rounds while keeping consolidated structural weights intact
  for (auto &row : hebbian_.plasticTraces) for (float &v : row) v *= 0.65f;
  liquid_.state.fill (0.0f);
  increment (stats_.rounds);
  dirty_ = true;
}

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

  std::vector<uint8_t> b2 {'H','C','L','R','N','M','L','5'};
  put32 (b2, env); put32 (b2, ContinuousFeatureDim); put32 (b2, MlpHiddenDim); put32 (b2, static_cast<uint32_t> (map.size ()));
  b2.insert (b2.end (), map.begin (), map.end ());
  for (const auto &row : mlp_.w1) for (float v : row) putFloat (b2, v);
  for (float v : mlp_.b1) putFloat (b2, v);
  for (const auto &row : mlp_.w2) for (float v : row) putFloat (b2, v);
  for (float v : mlp_.b2) putFloat (b2, v);
  for (float v : mlp_.vw) putFloat (b2, v);
  putFloat (b2, mlp_.vb);
  for (const auto &row : mlp_.mw) for (float v : row) putFloat (b2, v);
  for (float v : mlp_.mb) putFloat (b2, v);
  put32 (b2, mlp_.steps);
  putFloat (b2, mlp_.avgTdError);
  for (const auto & p : players_) {
    put32 (b2, p.token); put32 (b2, p.team);
    putFloat (b2, p.aggression); putFloat (b2, p.stationary); putFloat (b2, p.strafeRate);
    putFloat (b2, p.crouchRate); putFloat (b2, p.awpPreference); putFloat (b2, p.duelWinRate);
    putFloat (b2, p.lastPos.x); putFloat (b2, p.lastPos.y); putFloat (b2, p.lastPos.z);
    put32 (b2, p.sightings); put32 (b2, p.duels);
  }
  for (const auto &row : hrl_.wOption) for (float v : row) putFloat (b2, v);
  for (float v : hrl_.bOption) putFloat (b2, v);
  for (float v : hrl_.optionValue) putFloat (b2, v);
  put32 (b2, hrl_.switches);
  put32 (b2, hrl_.updates);
  for (const auto &actMat : world_.wDyn) for (const auto &row : actMat) for (float v : row) putFloat (b2, v);
  for (const auto &row : world_.bDyn) for (float v : row) putFloat (b2, v);
  for (const auto &row : world_.wRew) for (float v : row) putFloat (b2, v);
  for (float v : world_.bRew) putFloat (b2, v);
  for (float v : world_.wVal) putFloat (b2, v);
  putFloat (b2, world_.bVal);
  put32 (b2, world_.trainSteps);
  put32 (b2, world_.rollouts);
  putFloat (b2, world_.avgModelLoss);
  for (const auto &row : curiosity_.wPred) for (float v : row) putFloat (b2, v);
  for (float v : curiosity_.bPred) putFloat (b2, v);
  put32 (b2, curiosity_.steps);
  putFloat (b2, curiosity_.avgCuriosity);
  // Level 5 Liquid Recurrent Core & Self-Modifying Hebbian Cortex persistence
  for (float v : liquid_.state) putFloat (b2, v);
  for (const auto &row : liquid_.wIn) for (float v : row) putFloat (b2, v);
  for (const auto &row : liquid_.wRec) for (float v : row) putFloat (b2, v);
  for (float v : liquid_.bRec) putFloat (b2, v);
  for (float v : liquid_.tauBase) putFloat (b2, v);
  for (const auto &row : liquid_.wReadout) for (float v : row) putFloat (b2, v);
  put32 (b2, liquid_.ticks);
  for (const auto &row : hebbian_.plasticTraces) for (float v : row) putFloat (b2, v);
  for (const auto &row : hebbian_.synapseUtility) for (float v : row) putFloat (b2, v);
  for (const auto &row : hebbian_.activeMask) for (uint8_t m : row) put32 (b2, m);
  for (float v : hebbian_.coeffA) putFloat (b2, v);
  for (float v : hebbian_.coeffB) putFloat (b2, v);
  for (float v : hebbian_.coeffC) putFloat (b2, v);
  for (float v : hebbian_.coeffD) putFloat (b2, v);
  for (float v : hebbian_.eta) putFloat (b2, v);
  put32 (b2, hebbian_.activeSynapses);
  put32 (b2, hebbian_.prunedSynapses);
  put32 (b2, hebbian_.sproutedSynapses);
  put32 (b2, hebbian_.plasticitySteps);
  put32 (b2, hebbian_.topologyRewires);
  put32 (b2, hashBytes (b2.data (), b2.size ()));
  const auto path2 = path + "2";
  const auto temp2 = path2 + ".tmp";
  if (auto f2 = std::fopen (temp2.c_str (), "wb")) {
    bool ok2 = std::fwrite (b2.data (), 1, b2.size (), f2) == b2.size ();
    if (std::fflush (f2) != 0) ok2 = false;
    if (std::fclose (f2) != 0) ok2 = false;
    if (ok2) {
#ifdef _WIN32
      MoveFileExA (temp2.c_str (), path2.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
      std::rename (temp2.c_str (), path2.c_str ());
#endif
    }
    else std::remove (temp2.c_str ());
  }

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

  const auto path2 = path + "2";
  if (auto f2 = std::fopen (path2.c_str (), "rb")) {
    std::vector<uint8_t> b2 (65536);
    const auto sz2 = std::fread (b2.data (), 1, b2.size (), f2);
    const bool err2 = std::ferror (f2) != 0; std::fclose (f2);
    const bool v1 = !err2 && sz2 > 32 && sz2 < b2.size () && std::memcmp (b2.data (), "HCLRNMLP", 8) == 0;
    const bool v2 = !err2 && sz2 > 32 && sz2 < b2.size () && std::memcmp (b2.data (), "HCLRNML2", 8) == 0;
    const bool v4 = !err2 && sz2 > 32 && sz2 < b2.size () && std::memcmp (b2.data (), "HCLRNML4", 8) == 0;
    const bool v5 = !err2 && sz2 > 32 && sz2 < b2.size () && std::memcmp (b2.data (), "HCLRNML5", 8) == 0;
    if (v1 || v2 || v4 || v5) {
      b2.resize (sz2); Reader r2 {b2}; r2.pos = 8;
      const auto env2 = r2.number (), fDim = r2.number (), hDim = r2.number (), mSz = r2.number ();
      Reader tail2 {b2}; tail2.pos = sz2 - 4;
      if (env2 == env && fDim == ContinuousFeatureDim && hDim == MlpHiddenDim && mSz == map.size () &&
          sz2 >= 24 + mSz + 4 && std::memcmp (b2.data () + 24, map.data (), mSz) == 0 &&
          tail2.number () == hashBytes (b2.data (), sz2 - 4)) {
        r2.pos += mSz;
        TinyMlpPolicy mlpCand {};
        bool mlpOk = true;
        auto readChecked = [&] (float &out, float limit) {
          out = r2.real ();
          if (!r2.ok || !std::isfinite (out) || std::fabs (out) > limit) mlpOk = false;
        };
        for (auto &row : mlpCand.w1) for (float &v : row) readChecked (v, 10.0f);
        for (float &v : mlpCand.b1) readChecked (v, 10.0f);
        for (auto &row : mlpCand.w2) for (float &v : row) readChecked (v, 10.0f);
        for (float &v : mlpCand.b2) readChecked (v, 10.0f);
        for (float &v : mlpCand.vw) readChecked (v, 10.0f);
        readChecked (mlpCand.vb, 20.0f);
        for (auto &row : mlpCand.mw) for (float &v : row) readChecked (v, 10.0f);
        for (float &v : mlpCand.mb) readChecked (v, 10.0f);
        mlpCand.steps = r2.number ();
        readChecked (mlpCand.avgTdError, 20.0f);
        std::array<OpponentPlayerProfile, MaxTrackedPlayers> playersCand {};
        for (auto &p : playersCand) {
          p.token = r2.number (); p.team = r2.number ();
          readChecked (p.aggression, 2.0f); readChecked (p.stationary, 2.0f); readChecked (p.strafeRate, 2.0f);
          readChecked (p.crouchRate, 2.0f); readChecked (p.awpPreference, 2.0f); readChecked (p.duelWinRate, 2.0f);
          readChecked (p.lastPos.x, 65536.0f); readChecked (p.lastPos.y, 65536.0f); readChecked (p.lastPos.z, 65536.0f);
          p.sightings = r2.number (); p.duels = r2.number ();
        }
        HrlOptionPolicy hrlCand = candidate.hrl_;
        if (v2 || v4 || v5) {
          for (auto &row : hrlCand.wOption) for (float &v : row) readChecked (v, 10.0f);
          for (float &v : hrlCand.bOption) readChecked (v, 10.0f);
          for (float &v : hrlCand.optionValue) readChecked (v, 15.0f);
          hrlCand.switches = r2.number ();
          hrlCand.updates = r2.number ();
        }
        LatentWorldModel worldCand = candidate.world_;
        IntrinsicCuriosityModule curCand = candidate.curiosity_;
        if (v4 || v5) {
          for (auto &actMat : worldCand.wDyn) for (auto &row : actMat) for (float &v : row) readChecked (v, 10.0f);
          for (auto &row : worldCand.bDyn) for (float &v : row) readChecked (v, 10.0f);
          for (auto &row : worldCand.wRew) for (float &v : row) readChecked (v, 10.0f);
          for (float &v : worldCand.bRew) readChecked (v, 10.0f);
          for (float &v : worldCand.wVal) readChecked (v, 10.0f);
          readChecked (worldCand.bVal, 15.0f);
          worldCand.trainSteps = r2.number ();
          worldCand.rollouts = r2.number ();
          readChecked (worldCand.avgModelLoss, 50.0f);
          for (auto &row : curCand.wPred) for (float &v : row) readChecked (v, 10.0f);
          for (float &v : curCand.bPred) readChecked (v, 10.0f);
          curCand.steps = r2.number ();
          readChecked (curCand.avgCuriosity, 10.0f);
        }
        LiquidRecurrentCore liqCand = candidate.liquid_;
        HebbianPlasticCortex hebCand = candidate.hebbian_;
        if (v5) {
          for (float &v : liqCand.state) readChecked (v, 5.0f);
          for (auto &row : liqCand.wIn) for (float &v : row) readChecked (v, 10.0f);
          for (auto &row : liqCand.wRec) for (float &v : row) readChecked (v, 10.0f);
          for (float &v : liqCand.bRec) readChecked (v, 10.0f);
          for (float &v : liqCand.tauBase) readChecked (v, 10.0f);
          for (auto &row : liqCand.wReadout) for (float &v : row) readChecked (v, 10.0f);
          liqCand.ticks = r2.number ();
          for (auto &row : hebCand.plasticTraces) for (float &v : row) readChecked (v, 10.0f);
          for (auto &row : hebCand.synapseUtility) for (float &v : row) readChecked (v, 10.0f);
          uint32_t maskCount = 0;
          for (auto &row : hebCand.activeMask) for (uint8_t &m : row) {
            const uint32_t mv = r2.number ();
            m = mv ? 1u : 0u;
            if (m) ++maskCount;
          }
          for (float &v : hebCand.coeffA) readChecked (v, 5.0f);
          for (float &v : hebCand.coeffB) readChecked (v, 5.0f);
          for (float &v : hebCand.coeffC) readChecked (v, 5.0f);
          for (float &v : hebCand.coeffD) readChecked (v, 5.0f);
          for (float &v : hebCand.eta) readChecked (v, 5.0f);
          hebCand.activeSynapses = maskCount;
          (void)r2.number ();
          hebCand.prunedSynapses = r2.number ();
          hebCand.sproutedSynapses = r2.number ();
          hebCand.plasticitySteps = r2.number ();
          hebCand.topologyRewires = r2.number ();
        }
        if (mlpOk && r2.ok && r2.pos == sz2 - 4) {
          candidate.mlp_ = mlpCand;
          candidate.players_ = playersCand;
          candidate.hrl_ = hrlCand;
          candidate.world_ = worldCand;
          candidate.curiosity_ = curCand;
          candidate.liquid_ = liqCand;
          candidate.hebbian_ = hebCand;
        }
      }
    }
  }

  *this = candidate; dirty_ = false; return true;
}
} // namespace rbcc
#endif
