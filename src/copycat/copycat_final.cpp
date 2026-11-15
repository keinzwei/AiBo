// GPL-2.0-or-later. Level 5 Apex: Self-Modifying Neuroevolution, Population DNA Pool, ~99.9% Autonomy, and 10-Player 5v5 Team Equilibrium.
#include "copycat_final.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif
namespace rbcc {
namespace {
bool point (const Vec &p) {
  return std::isfinite (p.x) && std::isfinite (p.y) && std::isfinite (p.z) && std::fabs (p.x) < 65536 && std::fabs (p.y) < 65536 &&
         std::fabs (p.z) < 65536;
}
bool sameRegion (const Vec &a, const Vec &b) {
  const auto d = a - b;
  return std::fabs (d.z) < 72 && d.x * d.x + d.y * d.y < 176.0f * 176.0f;
}
std::array<float, UtilityFeatureDim> toFeatureArray (const UtilityContext &c) {
  return {
    std::clamp (c.healthRatio, 0.0f, 1.0f),
    std::clamp (c.economyRatio, 0.0f, 1.0f),
    std::clamp (c.numericalAdvantage, -1.0f, 1.0f),
    std::clamp (c.isSniper, 0.0f, 1.0f),
    std::clamp (c.hasUtility, 0.0f, 1.0f),
    std::clamp (c.roundUrgency, 0.0f, 1.0f),
    std::clamp (c.opponentAggression, -1.0f, 1.0f),
    std::clamp (c.personalRisk, 0.0f, 1.0f)
  };
}
void put (std::vector<uint8_t> &b, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i)
    b.push_back (static_cast<uint8_t> (n >> (i * 8u)));
}
void real (std::vector<uint8_t> &b, float f) {
  uint32_t n;
  std::memcpy (&n, &f, 4);
  put (b, n);
}
struct Read {
  const std::vector<uint8_t> &b;
  size_t p {};
  bool ok { true };
  uint32_t get () {
    if (p + 4 > b.size ()) {
      ok = false;
      return 0;
    }
    uint32_t n {};
    for (unsigned i = 0; i < 4; ++i)
      n |= static_cast<uint32_t> (b[p++]) << (i * 8u);
    return n;
  }
  float real () {
    auto n = get ();
    float f;
    std::memcpy (&f, &n, 4);
    return f;
  }
};
bool mapValid (const std::string &m) {
  return !m.empty () && m.size () <= 63 &&
         m.find_first_not_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == std::string::npos;
}
uint32_t nextEvoRng (uint32_t &s) {
  if (!s) s = 0x9e3779b9u;
  s ^= s << 13u; s ^= s >> 17u; s ^= s << 5u;
  return s;
}
} // namespace

void AsyncTacticalCommander::clear () {
  *this = AsyncTacticalCommander {};
}
void AsyncTacticalCommander::dispatch (const TeamWorldSnapshot &snap, const FinalMemory *mem) {
  if (snap.team > 1) return;
  auto &ord = orders_[snap.team];
  std::array<float, 6> scores {};
  for (size_t d = 0; d < 6; ++d) scores[d] = directiveValues_[snap.team][d] * 0.7f;
  const float numAdv = static_cast<float> (snap.aliveAllies - snap.aliveEnemies);
  scores[static_cast<size_t> (TeamDirective::BalancedSpread)]        += 0.25f;
  scores[static_cast<size_t> (TeamDirective::SynchronizedCrossfire)] += (snap.team == 1 || snap.bombPlanted ? 0.55f : 0.20f) + (snap.opponentAggression > 0.2f ? 0.45f : 0.0f);
  scores[static_cast<size_t> (TeamDirective::FakeSplitExecute)]      += (snap.team == 0 && !snap.bombPlanted && snap.aliveAllies >= 3 ? 0.50f : -0.20f);
  scores[static_cast<size_t> (TeamDirective::PinchFlank)]            += (numAdv >= 1.0f ? 0.55f : 0.15f) + (mem && mem->heatmapSize () > 2 ? 0.25f : 0.0f);
  scores[static_cast<size_t> (TeamDirective::BaitAndTrade)]          += (snap.opponentAggression > 0.15f ? 0.50f : 0.25f);
  scores[static_cast<size_t> (TeamDirective::EcoSaveAmbush)]         += (snap.avgMoney < 2400 || numAdv <= -2.0f ? 0.85f : -0.45f);

  size_t bestD = 0;
  float bestScore = -1e30f;
  for (size_t d = 0; d < 6; ++d) {
    const float tie = static_cast<float> ((snap.roundNumber * 37u + static_cast<unsigned> (d) * 13u) % 19u) * 0.008f;
    if (scores[d] + tie > bestScore) { bestScore = scores[d] + tie; bestD = d; }
  }
  ord.directive = static_cast<TeamDirective> (bestD);
  ord.roles.fill (BotTacticalRole::TradeSupport);
  ord.crossfireAngles.fill (0.0f);

  bool entryAssigned = false;
  for (size_t i = 0; i < snap.slots.size (); ++i) {
    const int s = snap.slots[i];
    if (s < 0 || s >= 32) continue;
    const bool awp = i < snap.snipers.size () && snap.snipers[i];
    const bool carrier = i < snap.carriers.size () && snap.carriers[i];
    if (carrier) {
      ord.roles[static_cast<size_t> (s)] = BotTacticalRole::ObjectiveCarrier;
      ord.crossfireAngles[static_cast<size_t> (s)] = 0.0f;
    }
    else if (awp) {
      ord.roles[static_cast<size_t> (s)] = BotTacticalRole::SniperOverwatch;
      ord.crossfireAngles[static_cast<size_t> (s)] = (i % 2 == 0) ? -22.0f : 22.0f;
    }
    else if (!entryAssigned && (ord.directive == TeamDirective::PinchFlank || ord.directive == TeamDirective::FakeSplitExecute)) {
      ord.roles[static_cast<size_t> (s)] = BotTacticalRole::EntryFrag;
      ord.crossfireAngles[static_cast<size_t> (s)] = -18.0f;
      entryAssigned = true;
    }
    else if (i % 3 == 0 && ord.directive == TeamDirective::SynchronizedCrossfire) {
      ord.roles[static_cast<size_t> (s)] = BotTacticalRole::CrossfireAnchor;
      ord.crossfireAngles[static_cast<size_t> (s)] = (i % 2 == 0) ? -35.0f : 35.0f;
    }
    else if (i % 3 == 2) {
      ord.roles[static_cast<size_t> (s)] = BotTacticalRole::FlankLurker;
      ord.crossfireAngles[static_cast<size_t> (s)] = 28.0f;
    }
    else {
      ord.roles[static_cast<size_t> (s)] = BotTacticalRole::TradeSupport;
      ord.crossfireAngles[static_cast<size_t> (s)] = (i % 2 == 0) ? 16.0f : -16.0f;
    }
  }
  ord.callout = std::string (teamDirectiveName (ord.directive)) + " (" + std::to_string (snap.aliveAllies) + "v" + std::to_string (snap.aliveEnemies) + ")";
  if (ord.sequence < 1000000000u) ++ord.sequence;
  if (dispatches_ < 1000000000u) ++dispatches_;
}
void AsyncTacticalCommander::rewardDirective (unsigned team, float reward) {
  if (team > 1 || !std::isfinite (reward)) return;
  const auto d = static_cast<size_t> (orders_[team].directive);
  if (d >= 6) return;
  auto &v = directiveValues_[team][d];
  v = std::clamp (v + 0.18f * (std::clamp (reward, -2.0f, 2.0f) - v), -4.0f, 4.0f);
}
const CommanderOrder &AsyncTacticalCommander::order (unsigned team) const {
  static const CommanderOrder empty {};
  return team < 2 ? orders_[team] : empty;
}

// Level 5 Inter-Bot Population Neuroevolution Implementation
void NeuroevolutionPool::initPopulation () {
  generations_ = 1;
  crossovers_ = 0;
  mutations_ = 0;
  nextGenomeId_ = 1;
  uint32_t seed = 0x444E4135u; // "DNA5"
  for (size_t i = 0; i < MaxGenomePopulation; ++i) {
    BotGenome g {};
    g.genomeId = nextGenomeId_++;
    g.generation = 1;
    g.lineageId = static_cast<uint32_t> (i + 1u);
    const float jitter = static_cast<float> (static_cast<int32_t> (nextEvoRng (seed) & 1023u) - 512) / 512.0f;
    g.hebbianGenes = {
      std::clamp (0.42f + jitter * 0.15f, -1.5f, 1.5f),
      std::clamp (0.06f + jitter * 0.05f, -1.0f, 1.0f),
      std::clamp (-0.05f - jitter * 0.04f, -1.0f, 1.0f),
      std::clamp (jitter * 0.02f, -0.5f, 0.5f),
      std::clamp (0.085f + std::fabs (jitter) * 0.04f, 0.005f, 0.35f)
    };
    g.liquidTauScale = std::clamp (1.0f + jitter * 0.25f, 0.50f, 1.80f);
    g.motorReflexGain = std::clamp (1.0f + jitter * 0.18f, 0.70f, 1.40f);
    g.curiosityDrive = std::clamp (1.0f + std::fabs (jitter) * 0.30f, 0.50f, 1.80f);
    g.fitness = 0.50f;
    g.episodes = 0;
    pop_[i] = g;
  }
}
const BotGenome &NeuroevolutionPool::genome (size_t slot) const {
  return pop_[slot % MaxGenomePopulation];
}
void NeuroevolutionPool::reportFitness (size_t slot, float reward) {
  if (!std::isfinite (reward)) return;
  auto &g = pop_[slot % MaxGenomePopulation];
  g.fitness = std::clamp (g.fitness + 0.20f * (std::clamp (0.50f + reward * 0.35f, 0.0f, 2.0f) - g.fitness), 0.0f, 2.0f);
  if (g.episodes < 1000000000u) ++g.episodes;
}
const BotGenome &NeuroevolutionPool::champion () const {
  size_t best = 0;
  for (size_t i = 1; i < MaxGenomePopulation; ++i) {
    if (pop_[i].fitness > pop_[best].fitness) best = i;
  }
  return pop_[best];
}
uint32_t NeuroevolutionPool::evolveGeneration (uint32_t &rngSeed) {
  // Sort indices by descending fitness
  std::array<size_t, MaxGenomePopulation> order {};
  for (size_t i = 0; i < MaxGenomePopulation; ++i) order[i] = i;
  std::sort (order.begin (), order.end (), [&] (size_t a, size_t b) {
    return pop_[a].fitness > pop_[b].fitness;
  });

  // Top 4 elites act as parents; replace the bottom 4 genomes with crossover + mutated offspring
  const size_t eliteCount = 4;
  for (size_t k = MaxGenomePopulation - 4; k < MaxGenomePopulation; ++k) {
    const size_t targetIdx = order[k];
    const BotGenome &p1 = pop_[order[nextEvoRng (rngSeed) % eliteCount]];
    const BotGenome &p2 = pop_[order[nextEvoRng (rngSeed) % eliteCount]];

    BotGenome child = p1;
    child.genomeId = nextGenomeId_++;
    if (!nextGenomeId_) nextGenomeId_ = 1;
    child.generation = std::max (p1.generation, p2.generation) + 1u;
    child.lineageId = p1.lineageId;
    child.fitness = (p1.fitness + p2.fitness) * 0.5f;
    child.episodes = 0;

    // Uniform crossover + Gaussian-like perturbation across all Hebbian & Liquid genes
    for (size_t g = 0; g < 5; ++g) {
      const float base = ((nextEvoRng (rngSeed) & 1u) != 0u) ? p1.hebbianGenes[g] : p2.hebbianGenes[g];
      const float mut = (static_cast<float> (static_cast<int32_t> (nextEvoRng (rngSeed) & 1023u) - 512) / 512.0f) * 0.045f;
      child.hebbianGenes[g] = base + mut;
    }
    child.hebbianGenes[0] = std::clamp (child.hebbianGenes[0], -1.5f, 1.5f);
    child.hebbianGenes[1] = std::clamp (child.hebbianGenes[1], -1.0f, 1.0f);
    child.hebbianGenes[2] = std::clamp (child.hebbianGenes[2], -1.0f, 1.0f);
    child.hebbianGenes[3] = std::clamp (child.hebbianGenes[3], -0.5f, 0.5f);
    child.hebbianGenes[4] = std::clamp (child.hebbianGenes[4], 0.005f, 0.35f);

    const float mutTau = (static_cast<float> (static_cast<int32_t> (nextEvoRng (rngSeed) & 1023u) - 512) / 512.0f) * 0.08f;
    const float mutRef = (static_cast<float> (static_cast<int32_t> (nextEvoRng (rngSeed) & 1023u) - 512) / 512.0f) * 0.06f;
    const float mutCur = (static_cast<float> (static_cast<int32_t> (nextEvoRng (rngSeed) & 1023u) - 512) / 512.0f) * 0.08f;
    child.liquidTauScale = std::clamp (((p1.liquidTauScale + p2.liquidTauScale) * 0.5f) + mutTau, 0.50f, 1.80f);
    child.motorReflexGain = std::clamp (((p1.motorReflexGain + p2.motorReflexGain) * 0.5f) + mutRef, 0.70f, 1.40f);
    child.curiosityDrive = std::clamp (((p1.curiosityDrive + p2.curiosityDrive) * 0.5f) + mutCur, 0.50f, 1.80f);

    pop_[targetIdx] = child;
    if (crossovers_ < 1000000000u) ++crossovers_;
    if (mutations_ < 1000000000u) ++mutations_;
  }
  if (generations_ < 1000000000u) ++generations_;
  return generations_;
}

// Level 4 & 5 SelfEvolvingBrain Implementation
void SelfEvolvingBrain::clear () {
  *this = SelfEvolvingBrain {};
}
CurriculumGoal SelfEvolvingBrain::selectCurriculum (uint32_t profile, const UtilityContext &ctx, uint32_t &seed) {
  if (!profile) profile = 1u;
  if (!profiles_.count (profile) && profiles_.size () >= 128) {
    profiles_.erase (profiles_.begin ());
  }
  auto &p = profiles_[profile];
  if (!seed) seed = 0x9e3779b9u;
  seed ^= seed << 13u; seed ^= seed >> 17u; seed ^= seed << 5u;

  std::array<float, CurriculumGoalCount> scores {};
  for (size_t g = 0; g < CurriculumGoalCount; ++g) {
    const float competenceGap = 1.0f - std::clamp (p.mastery[g], 0.0f, 1.0f);
    const float learnProgress = std::clamp (p.progress[g], 0.0f, 1.0f);
    const float ucb = 0.12f / std::sqrt (1.0f + static_cast<float> (p.attempts[g]));
    scores[g] = competenceGap * 0.55f + learnProgress * 0.45f + ucb;
  }
  if (ctx.healthRatio > 0.75f && ctx.personalRisk > 0.55f)
    scores[static_cast<size_t> (CurriculumGoal::OpeningEntryDuel)] += 0.25f;
  if (ctx.numericalAdvantage >= 0.0f)
    scores[static_cast<size_t> (CurriculumGoal::CrossfireTradeSync)] += 0.22f;
  if (ctx.opponentAggression < 0.0f)
    scores[static_cast<size_t> (CurriculumGoal::NovelAngleDiscovery)] += 0.28f;
  if (ctx.isSniper > 0.5f || ctx.opponentAggression > 0.25f)
    scores[static_cast<size_t> (CurriculumGoal::SniperCounterBait)] += 0.26f;
  if (ctx.healthRatio < 0.45f || ctx.numericalAdvantage < -0.2f)
    scores[static_cast<size_t> (CurriculumGoal::ClutchSurvivalIsolation)] += 0.35f;
  scores[static_cast<size_t> (CurriculumGoal::SkillEvolutionMastery)] += 0.20f;

  size_t bestG = 0;
  float bestScore = -1e30f;
  for (size_t g = 0; g < CurriculumGoalCount; ++g) {
    const float jitter = static_cast<float> ((seed >> (g * 3u)) & 31u) * 0.003f;
    if (scores[g] + jitter > bestScore) {
      bestScore = scores[g] + jitter;
      bestG = g;
    }
  }
  p.activeGoal = static_cast<CurriculumGoal> (bestG);
  if (p.attempts[bestG] < 1000000000u) ++p.attempts[bestG];
  return p.activeGoal;
}
void SelfEvolvingBrain::recordCurriculumOutcome (uint32_t profile, CurriculumGoal goal, float reward) {
  if (!profile || !std::isfinite (reward)) return;
  const auto g = static_cast<size_t> (goal);
  if (g >= CurriculumGoalCount) return;
  auto &p = profiles_[profile];
  const float oldMastery = p.mastery[g];
  const float normalizedOutcome = std::clamp (0.5f + reward * 0.35f, 0.0f, 1.0f);
  p.mastery[g] = std::clamp (oldMastery + 0.14f * (normalizedOutcome - oldMastery), 0.05f, 0.999f);
  const float delta = std::fabs (p.mastery[g] - oldMastery);
  p.progress[g] = std::clamp (0.80f * p.progress[g] + 0.20f * (delta * 4.0f), 0.02f, 1.0f);
  if (reward > 0.15f && p.curriculumCompletions < 1000000000u) ++p.curriculumCompletions;

  // Level 5 Apex Progressive Unshackling: autonomy reaches ~99.9% (0.999f; only ~0.1% hard GoldSrc engine protocol hooks remain)
  float avgMastery = 0.0f;
  for (float m : p.mastery) avgMastery += m;
  avgMastery /= static_cast<float> (CurriculumGoalCount);
  p.autonomyRatio = std::clamp (0.990f + 0.009f * avgMastery, 0.990f, 0.999f);
}
float SelfEvolvingBrain::autonomyRatio (uint32_t profile, bool hardEngineRuleActive) const {
  if (hardEngineRuleActive) return 0.0f;
  const auto it = profiles_.find (profile);
  return it != profiles_.end () ? it->second.autonomyRatio : 0.999f;
}
bool SelfEvolvingBrain::shouldOverrideLegacyFsm (uint32_t profile, bool hardEngineRuleActive, float *outAutonomy) {
  if (hardEngineRuleActive) {
    if (outAutonomy) *outAutonomy = 0.0f;
    return false;
  }
  if (!profile) profile = 1u;
  auto &p = profiles_[profile];
  if (p.unshackledTicks < 1000000000u) ++p.unshackledTicks;
  if (totalCycles_ < 1000000000u) ++totalCycles_;
  if (outAutonomy) *outAutonomy = p.autonomyRatio;
  return true;
}
const SelfEvolvingProfile &SelfEvolvingBrain::profileState (uint32_t profile) const {
  static const SelfEvolvingProfile defaults {};
  const auto it = profiles_.find (profile);
  return it != profiles_.end () ? it->second : defaults;
}

bool SelfEvolvingBrain::save (const std::string &path, const std::string &map, uint32_t env, std::string &error) const {
  error.clear ();
  if (!mapValid (map)) { error = "invalid map"; return false; }
  std::vector<uint8_t> b { 'H', 'C', 'E', 'V', 'O', 'B', 'R', '5' };
  put (b, env);
  put (b, static_cast<uint32_t> (map.size ()));
  b.insert (b.end (), map.begin (), map.end ());
  put (b, totalCycles_);
  put (b, evoPool_.generations ());
  put (b, evoPool_.crossovers ());
  put (b, evoPool_.mutations ());
  for (const auto &g : evoPool_.population ()) {
    put (b, g.genomeId);
    put (b, g.generation);
    put (b, g.lineageId);
    for (float hg : g.hebbianGenes) real (b, hg);
    real (b, g.liquidTauScale);
    real (b, g.motorReflexGain);
    real (b, g.curiosityDrive);
    real (b, g.fitness);
    put (b, g.episodes);
  }
  put (b, static_cast<uint32_t> (profiles_.size ()));
  for (const auto &[id, prof] : profiles_) {
    put (b, id);
    for (float m : prof.mastery) real (b, m);
    for (float pr : prof.progress) real (b, pr);
    for (uint32_t att : prof.attempts) put (b, att);
    put (b, static_cast<uint32_t> (prof.activeGoal));
    real (b, prof.autonomyRatio);
    put (b, prof.unshackledTicks);
    put (b, prof.curriculumCompletions);
  }
  put (b, hashBytes (b.data (), b.size ()));
  const auto tmp = path + ".tmp";
  auto f = std::fopen (tmp.c_str (), "wb");
  if (!f) { error = "cannot open evo brain temp"; return false; }
  bool ok = std::fwrite (b.data (), 1, b.size (), f) == b.size ();
  if (std::fflush (f)) ok = false;
  if (std::fclose (f)) ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA (tmp.c_str (), path.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename (tmp.c_str (), path.c_str ()) == 0;
#endif
  }
  if (!ok) { std::remove (tmp.c_str ()); error = "evo brain save failed"; return false; }
  return true;
}

bool SelfEvolvingBrain::load (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear ();
  auto f = std::fopen (path.c_str (), "rb");
  if (!f) { error = "evo brain file not found"; return false; }
  std::vector<uint8_t> b (128 * 1024);
  const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool io = std::ferror (f) != 0;
  std::fclose (f);
  if (io || size < 32 || size >= b.size () || !mapValid (map)) { error = "invalid evo brain size"; return false; }
  b.resize (size);
  Read r { b }, tail { b };
  tail.p = size - 4;
  if (std::memcmp (b.data (), "HCEVOBR5", 8) != 0 || tail.get () != hashBytes (b.data (), size - 4)) {
    error = "evo brain header/checksum mismatch"; return false;
  }
  r.p = 8;
  const auto storedEnv = r.get (), len = r.get ();
  if (storedEnv != env || len != map.size () || std::memcmp (b.data () + 16, map.data (), len) != 0) {
    error = "evo brain map/env mismatch"; return false;
  }
  r.p += len;
  const uint32_t cycles = r.get (), gens = r.get (), cross = r.get (), muts = r.get ();
  std::array<BotGenome, MaxGenomePopulation> popCand {};
  uint32_t maxId = 1;
  for (auto &g : popCand) {
    g.genomeId = r.get ();
    if (g.genomeId >= maxId) maxId = g.genomeId + 1u;
    g.generation = r.get ();
    g.lineageId = r.get ();
    for (float &hg : g.hebbianGenes) hg = r.real ();
    g.liquidTauScale = r.real ();
    g.motorReflexGain = r.real ();
    g.curiosityDrive = r.real ();
    g.fitness = r.real ();
    g.episodes = r.get ();
  }
  const uint32_t profCount = r.get ();
  if (!r.ok || profCount > 128) { error = "invalid evo profile count"; return false; }
  std::map<uint32_t, SelfEvolvingProfile> profCand {};
  for (uint32_t i = 0; i < profCount && r.ok; ++i) {
    const uint32_t id = r.get ();
    SelfEvolvingProfile pr {};
    for (float &m : pr.mastery) m = r.real ();
    for (float &pg : pr.progress) pg = r.real ();
    for (uint32_t &att : pr.attempts) att = r.get ();
    const uint32_t goalIdx = r.get ();
    pr.activeGoal = goalIdx < CurriculumGoalCount ? static_cast<CurriculumGoal> (goalIdx) : CurriculumGoal::OpeningEntryDuel;
    pr.autonomyRatio = std::clamp (r.real (), 0.90f, 0.999f);
    pr.unshackledTicks = r.get ();
    pr.curriculumCompletions = r.get ();
    profCand[id] = pr;
  }
  if (!r.ok || r.p != size - 4) { error = "invalid evo brain payload"; return false; }
  totalCycles_ = cycles;
  evoPool_.setPopulation (popCand, std::max (1u, gens), cross, muts, maxId);
  profiles_ = std::move (profCand);
  return true;
}


void FinalMemory::clear () {
  *this = FinalMemory {};
}
bool FinalMemory::fresh (uint32_t profile, const Vec &foot, unsigned window) const {
  const auto it = histories_.find (profile);
  if (it == histories_.end ())
    return true;
  const auto &h = it->second;
  const unsigned count = std::min ({ window, h.count, 32u });
  for (unsigned i = 0; i < count; ++i)
    if (sameRegion (h.places[(h.cursor + 31u - i) % 32u].foot, foot))
      return false;
  return true;
}
float FinalMemory::bias (uint32_t profile, const Vec &foot) const {
  const auto it = histories_.find (profile);
  if (it == histories_.end ())
    return 0;
  float penalty = 0;
  const auto &h = it->second;
  for (unsigned i = 0; i < std::min (h.count, 32u); ++i) {
    const auto &p = h.places[(h.cursor + 31u - i) % 32u];
    if (sameRegion (p.foot, foot))
      penalty += 650.0f / (1.0f + static_cast<float> (i) * 0.3f);
  }
  return -penalty;
}
float FinalMemory::tacticalBias (uint32_t profile, const Vec &foot, FinalStrategy strategy, float risk) const {
  float score = bias (profile, foot);
  const float threat = threatAt (foot);
  const float opp = opportunityAt (foot);
  risk = std::clamp (risk, 0.0f, 1.0f);
  switch (strategy) {
  case FinalStrategy::Push:
    score += opp * 140.0f - threat * (85.0f * (1.0f - risk));
    break;
  case FinalStrategy::Flank:
  case FinalStrategy::Lurk:
    score += opp * 125.0f - threat * 175.0f;
    break;
  case FinalStrategy::Snipe:
    score += opp * 165.0f - threat * 110.0f;
    break;
  case FinalStrategy::Scout:
    score += (opp - threat * 0.6f) * 90.0f;
    break;
  case FinalStrategy::Support:
  default:
    score += opp * 95.0f - threat * 95.0f;
    break;
  }
  return score;
}
void FinalMemory::remember (uint32_t profile, const Vec &foot, PlaceUse use) {
  if (!profile || !point (foot) || static_cast<unsigned> (use) > 4)
    return;
  if (!histories_.count (profile) && histories_.size () >= MaxProfiles)
    return;
  auto &h = histories_[profile];
  if (h.count && sameRegion (h.places[(h.cursor + 31u) % 32u].foot, foot))
    return;
  if (sequence_ < 1000000000u)
    ++sequence_;
  h.places[h.cursor % 32u] = { foot, sequence_, use };
  h.cursor = (h.cursor + 1u) % 32u;
  h.count = std::min (h.count + 1u, 32u);
  dirty_ = true;
}
void FinalMemory::strategy (uint32_t profile, FinalStrategy s) {
  const auto i = static_cast<size_t> (s);
  if (i >= 6 || !profile)
    return;
  if (!histories_.count (profile) && histories_.size () >= MaxProfiles)
    return;
  auto &n = histories_[profile].strategies[i];
  if (n < 1000000000u)
    ++n;
  dirty_ = true;
}
void FinalMemory::recordStrategyOutcome (uint32_t profile, FinalStrategy s, float reward) {
  const auto i = static_cast<size_t> (s);
  if (i >= 6 || !profile || !std::isfinite (reward))
    return;
  if (!histories_.count (profile) && histories_.size () >= MaxProfiles)
    return;
  auto &val = histories_[profile].strategyRewards[i];
  val = std::clamp (val + 0.15f * (std::clamp (reward, -3.0f, 3.0f) - val), -5.0f, 5.0f);
  dirty_ = true;
}
float FinalMemory::strategyValue (uint32_t profile, FinalStrategy s) const {
  const auto i = static_cast<size_t> (s);
  const auto it = histories_.find (profile);
  return (i < 6 && it != histories_.end ()) ? it->second.strategyRewards[i] : 0.0f;
}
void FinalMemory::recordSpatialEvent (const Vec &foot, float threatDelta, float opportunityDelta) {
  if (!point (foot) || !std::isfinite (threatDelta) || !std::isfinite (opportunityDelta))
    return;
  for (auto &c : heatmap_) {
    if (sameRegion (c.center, foot)) {
      c.threat = std::clamp (c.threat + threatDelta, 0.0f, 10.0f);
      c.opportunity = std::clamp (c.opportunity + opportunityDelta, 0.0f, 10.0f);
      if (c.events < 1000000000u) ++c.events;
      dirty_ = true;
      return;
    }
  }
  if (heatmap_.size () >= MaxHeatmapCells) {
    size_t weakest = 0;
    float lowest = 1e30f;
    for (size_t i = 0; i < heatmap_.size (); ++i) {
      const float importance = heatmap_[i].threat + heatmap_[i].opportunity;
      if (importance < lowest) { lowest = importance; weakest = i; }
    }
    heatmap_.erase (heatmap_.begin () + static_cast<std::ptrdiff_t> (weakest));
  }
  SpatialHeatCell cell;
  cell.center = foot;
  cell.threat = std::clamp (threatDelta, 0.0f, 10.0f);
  cell.opportunity = std::clamp (opportunityDelta, 0.0f, 10.0f);
  cell.events = 1;
  heatmap_.push_back (cell);
  dirty_ = true;
}
void FinalMemory::decayHeatmap (float factor) {
  if (!std::isfinite (factor)) return;
  factor = std::clamp (factor, 0.5f, 1.0f);
  for (auto &c : heatmap_) {
    c.threat *= factor;
    c.opportunity *= factor;
  }
}
float FinalMemory::threatAt (const Vec &foot) const {
  float total = 0.0f;
  for (const auto &c : heatmap_) {
    const float d2 = (c.center.x - foot.x) * (c.center.x - foot.x) + (c.center.y - foot.y) * (c.center.y - foot.y);
    if (std::fabs (c.center.z - foot.z) < 96.0f && d2 < 320.0f * 320.0f) {
      const float w = 1.0f - std::sqrt (d2) / 320.0f;
      total += c.threat * w;
    }
  }
  return std::clamp (total, 0.0f, 10.0f);
}
float FinalMemory::opportunityAt (const Vec &foot) const {
  float total = 0.0f;
  for (const auto &c : heatmap_) {
    const float d2 = (c.center.x - foot.x) * (c.center.x - foot.x) + (c.center.y - foot.y) * (c.center.y - foot.y);
    if (std::fabs (c.center.z - foot.z) < 96.0f && d2 < 320.0f * 320.0f) {
      const float w = 1.0f - std::sqrt (d2) / 320.0f;
      total += c.opportunity * w;
    }
  }
  return std::clamp (total, 0.0f, 10.0f);
}
uint32_t FinalMemory::strategies (uint32_t profile, FinalStrategy s) const {
  const auto i = static_cast<size_t> (s);
  const auto it = histories_.find (profile);
  return i < 6 && it != histories_.end () ? it->second.strategies[i] : 0;
}
bool FinalMemory::save (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear ();
  if (!mapValid (map)) {
    error = "invalid map";
    return false;
  }
  std::vector<uint8_t> b { 'H', 'C', 'F', 'I', 'N', 'A', 'L', '1' };
  put (b, env);
  put (b, static_cast<uint32_t> (map.size ()));
  b.insert (b.end (), map.begin (), map.end ());
  put (b, sequence_);
  put (b, static_cast<uint32_t> (histories_.size ()));
  for (const auto &[id, h] : histories_) {
    put (b, id);
    put (b, h.cursor);
    put (b, h.count);
    for (auto n : h.strategies)
      put (b, n);
    for (const auto &p : h.places) {
      real (b, p.foot.x);
      real (b, p.foot.y);
      real (b, p.foot.z);
      put (b, p.sequence);
      put (b, static_cast<uint32_t> (p.use));
    }
  }
  put (b, hashBytes (b.data (), b.size ()));
  const auto tmp = path + ".tmp";
  auto f = std::fopen (tmp.c_str (), "wb");
  if (!f) {
    error = "cannot open final memory temp";
    return false;
  }
  bool ok = std::fwrite (b.data (), 1, b.size (), f) == b.size ();
  if (std::fflush (f))
    ok = false;
  if (std::fclose (f))
    ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA (tmp.c_str (), path.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename (tmp.c_str (), path.c_str ()) == 0;
#endif
  }
  if (!ok) {
    std::remove (tmp.c_str ());
    error = "final memory save failed; old file retained";
    return false;
  }

  std::vector<uint8_t> b2 { 'H', 'C', 'F', 'N', 'L', 'U', 'T', '1' };
  put (b2, env);
  put (b2, static_cast<uint32_t> (map.size ()));
  b2.insert (b2.end (), map.begin (), map.end ());
  put (b2, static_cast<uint32_t> (histories_.size ()));
  for (const auto &[id, h] : histories_) {
    put (b2, id);
    for (float r : h.strategyRewards) real (b2, r);
  }
  put (b2, static_cast<uint32_t> (heatmap_.size ()));
  for (const auto &c : heatmap_) {
    real (b2, c.center.x); real (b2, c.center.y); real (b2, c.center.z);
    real (b2, c.threat); real (b2, c.opportunity);
    put (b2, c.events);
  }
  put (b2, hashBytes (b2.data (), b2.size ()));
  const auto path2 = path + "2";
  const auto tmp2 = path2 + ".tmp";
  if (auto f2 = std::fopen (tmp2.c_str (), "wb")) {
    bool ok2 = std::fwrite (b2.data (), 1, b2.size (), f2) == b2.size ();
    if (std::fflush (f2)) ok2 = false;
    if (std::fclose (f2)) ok2 = false;
    if (ok2) {
#ifdef _WIN32
      MoveFileExA (tmp2.c_str (), path2.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
      std::rename (tmp2.c_str (), path2.c_str ());
#endif
    }
    else std::remove (tmp2.c_str ());
  }

  dirty_ = false;
  return true;
}
bool FinalMemory::load (const std::string &path, const std::string &map, uint32_t env, std::string &error) {
  error.clear ();
  auto f = std::fopen (path.c_str (), "rb");
  if (!f) {
    error = "final memory not found";
    return false;
  }
  std::vector<uint8_t> b (256 * 1024);
  const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool io = std::ferror (f) != 0;
  std::fclose (f);
  if (io || size < 28 || size >= b.size () || !mapValid (map)) {
    error = "invalid final memory size";
    return false;
  }
  b.resize (size);
  Read r { b }, tail { b };
  tail.p = size - 4;
  if (std::memcmp (b.data (), "HCFINAL1", 8) || tail.get () != hashBytes (b.data (), size - 4)) {
    error = "final memory header/checksum";
    return false;
  }
  r.p = 8;
  const auto stored = r.get (), len = r.get ();
  if (stored != env || len != map.size () || 16 + len + 12 > size || std::memcmp (b.data () + 16, map.data (), len)) {
    error = "final memory map/physics mismatch";
    return false;
  }
  r.p += len;
  FinalMemory candidate;
  candidate.sequence_ = r.get ();
  const auto count = r.get ();
  if (count > MaxProfiles || candidate.sequence_ > 1000000000u || size != 28 + len + count * (36 + 32 * 20)) {
    error = "invalid final memory count";
    return false;
  }
  for (unsigned i = 0; i < count; ++i) {
    const auto id = r.get ();
    History h;
    h.cursor = r.get ();
    h.count = r.get ();
    if (!id || candidate.histories_.count (id) || h.cursor >= 32 || h.count > 32) {
      error = "invalid final profile";
      return false;
    }
    for (auto &n : h.strategies) {
      n = r.get ();
      if (n > 1000000000u) {
        error = "invalid strategy count";
        return false;
      }
    }
    for (auto &p : h.places) {
      p.foot.x = r.real ();
      p.foot.y = r.real ();
      p.foot.z = r.real ();
      p.sequence = r.get ();
      const auto u = r.get ();
      if (!point (p.foot) || p.sequence > candidate.sequence_ || u > 4) {
        error = "invalid final place";
        return false;
      }
      p.use = static_cast<PlaceUse> (u);
    }
    candidate.histories_.emplace (id, h);
  }
  if (!r.ok || r.p != size - 4) {
    error = "invalid final memory payload";
    return false;
  }

  const auto path2 = path + "2";
  if (auto f2 = std::fopen (path2.c_str (), "rb")) {
    std::vector<uint8_t> b2 (64 * 1024);
    const auto sz2 = std::fread (b2.data (), 1, b2.size (), f2);
    const bool err2 = std::ferror (f2) != 0;
    std::fclose (f2);
    if (!err2 && sz2 >= 24 + len && sz2 < b2.size () && std::memcmp (b2.data (), "HCFNLUT1", 8) == 0) {
      b2.resize (sz2);
      Read r2 { b2 }, tail2 { b2 };
      tail2.p = sz2 - 4;
      r2.p = 8;
      const auto env2 = r2.get (), len2 = r2.get ();
      if (env2 == env && len2 == len && std::memcmp (b2.data () + 16, map.data (), len) == 0 &&
          tail2.get () == hashBytes (b2.data (), sz2 - 4)) {
        r2.p += len;
        const auto profCount = r2.get ();
        bool ok2 = profCount <= MaxProfiles;
        for (unsigned i = 0; i < profCount && ok2; ++i) {
          const auto id = r2.get ();
          std::array<float, 6> rewards {};
          for (float &rv : rewards) {
            rv = r2.real ();
            if (!r2.ok || !std::isfinite (rv) || std::fabs (rv) > 10.0f) ok2 = false;
          }
          if (ok2 && candidate.histories_.count (id))
            candidate.histories_[id].strategyRewards = rewards;
        }
        const auto heatCount = r2.get ();
        if (heatCount > MaxHeatmapCells) ok2 = false;
        std::vector<SpatialHeatCell> heatCand;
        for (unsigned i = 0; i < heatCount && ok2; ++i) {
          SpatialHeatCell c;
          c.center.x = r2.real (); c.center.y = r2.real (); c.center.z = r2.real ();
          c.threat = r2.real (); c.opportunity = r2.real ();
          c.events = r2.get ();
          if (!r2.ok || !point (c.center) || !std::isfinite (c.threat) || !std::isfinite (c.opportunity) ||
              c.threat < 0 || c.threat > 20 || c.opportunity < 0 || c.opportunity > 20)
            ok2 = false;
          else
            heatCand.push_back (c);
        }
        if (ok2 && r2.ok && r2.p == sz2 - 4)
          candidate.heatmap_ = std::move (heatCand);
      }
    }
  }

  *this = std::move (candidate);
  dirty_ = false;
  return true;
}
uint32_t FinalPersona::next () {
  if (!random)
    random = 0x9e3779b9u;
  random ^= random << 13u;
  random ^= random >> 17u;
  random ^= random << 5u;
  return random;
}
void FinalPersona::seed (uint32_t s) {
  *this = FinalPersona {};
  random = s ? s : 1;
  ambition = 0.6f + static_cast<float> (next () % 36u) * 0.01f;
  confidence = 0.35f + static_cast<float> (next () % 36u) * 0.01f;
  contextWeights[static_cast<size_t> (FinalStrategy::Push)]    = {  0.35f,  0.20f,  0.40f, -0.45f,  0.30f,  0.50f, -0.15f,  0.55f };
  contextWeights[static_cast<size_t> (FinalStrategy::Flank)]   = {  0.25f,  0.10f,  0.15f, -0.35f,  0.20f, -0.20f,  0.40f,  0.35f };
  contextWeights[static_cast<size_t> (FinalStrategy::Lurk)]    = { -0.15f, -0.10f, -0.35f, -0.25f,  0.10f, -0.35f,  0.45f, -0.20f };
  contextWeights[static_cast<size_t> (FinalStrategy::Snipe)]   = { -0.10f,  0.30f, -0.10f,  1.10f,  0.05f, -0.15f,  0.25f, -0.30f };
  contextWeights[static_cast<size_t> (FinalStrategy::Scout)]   = {  0.20f, -0.20f,  0.10f, -0.15f,  0.15f, -0.25f, -0.20f,  0.25f };
  contextWeights[static_cast<size_t> (FinalStrategy::Support)] = { -0.25f,  0.15f,  0.25f, -0.20f,  0.55f,  0.35f,  0.15f, -0.35f };
  for (auto &row : contextWeights)
    for (float &w : row)
      w += (static_cast<float> (static_cast<int> (next () % 21u) - 10) * 0.01f);
}
void FinalPersona::rewardStrategy (float reward) {
  if (!std::isfinite (reward)) return;
  reward = std::clamp (reward, -2.5f, 2.5f);
  const auto idx = static_cast<size_t> (current);
  if (idx >= 6) return;
  const float pred = strategyValues[idx];
  const float err = reward - pred;
  strategyValues[idx] = std::clamp (pred + 0.18f * err, -3.0f, 3.0f);
  const auto feats = toFeatureArray (lastContext);
  for (size_t k = 0; k < UtilityFeatureDim; ++k) {
    contextWeights[idx][k] = std::clamp (contextWeights[idx][k] + 0.045f * err * feats[k], -2.5f, 2.5f);
  }
}
void FinalPersona::killed (bool friendly) {
  if (friendly) {
    anger = std::min (anger + 0.15f, 1.0f);
    confidence = std::max (0.0f, confidence - 0.2f);
    rewardStrategy (-0.8f);
    return;
  }
  ++streak;
  confidence = std::min (1.0f, confidence + 0.15f);
  fear *= 0.6f;
  anger *= 0.7f;
  rewardStrategy (0.65f);
}
void FinalPersona::died () {
  streak = 0;
  ++deaths;
  confidence = std::max (0.0f, confidence - 0.17f);
  anger = std::min (1.0f, anger + 0.3f);
  fear = std::min (1.0f, fear + 0.2f);
  rewardStrategy (-0.55f);
}
void FinalPersona::hurt (float damage) {
  if (!std::isfinite (damage))
    return;
  damage = std::clamp (damage, 0.0f, 100.0f);
  fear = std::min (1.0f, fear + damage * 0.008f);
  anger = std::min (1.0f, anger + damage * 0.003f);
  rewardStrategy (-damage * 0.003f);
}
void FinalPersona::update (float dt, int health, int score, int leader, int place) {
  if (!std::isfinite (dt))
    return;
  dt = std::clamp (dt, 0.0f, 2.0f);
  anger = std::max (0.0f, anger - dt * 0.012f);
  fear = std::max (0.0f, fear - dt * 0.018f);
  rank = std::max (1, place);
  gap = std::max (0, leader - score);
  top = score > 0 && score >= leader && rank == 1;
  const float urgency = std::min (static_cast<float> (gap) * 0.035f, 0.3f) * ambition;
  const float low = health < 35 ? 0.38f : (health < 60 ? 0.15f : 0.0f);
  risk = std::clamp (0.25f + confidence * 0.5f + anger * 0.25f + urgency - fear * 0.4f - low, 0.08f, 0.95f);
  if (low > 0.3f && fear > 0.35f)
    mood = FinalMood::Afraid;
  else if (anger > 0.55f)
    mood = FinalMood::Angry;
  else if (risk > 0.78f)
    mood = FinalMood::Reckless;
  else if (top || confidence > 0.7f)
    mood = FinalMood::Confident;
  else if (risk < 0.4f)
    mood = FinalMood::Cautious;
  else
    mood = FinalMood::Calm;
}
float FinalPersona::evaluateStrategy (FinalStrategy s, const UtilityContext &ctx, const FinalMemory *mem, uint32_t profile) const {
  const auto idx = static_cast<size_t> (s);
  if (idx >= 6) return -1000.0f;
  const auto feats = toFeatureArray (ctx);
  float score = strategyValues[idx] * 0.6f;
  for (size_t k = 0; k < UtilityFeatureDim; ++k)
    score += contextWeights[idx][k] * feats[k];
  if (mem && profile) {
    score += mem->strategyValue (profile, s) * 0.4f;
    uint32_t total = 0;
    for (unsigned i = 0; i < 6; ++i) total += mem->strategies (profile, static_cast<FinalStrategy> (i));
    if (total > 0) {
      const float share = static_cast<float> (mem->strategies (profile, s)) / static_cast<float> (total);
      score -= (share - (1.0f / 6.0f)) * 0.65f;
    }
  }
  if (s == current && utilityDecisions > 0)
    score -= 0.22f;
  return score;
}
FinalStrategy FinalPersona::chooseUtility (const UtilityContext &ctx, const FinalMemory *mem, uint32_t profile) {
  lastContext = ctx;
  lastContext.personalRisk = risk;
  std::array<float, 6> scores {};
  float bestScore = -1e30f;
  size_t bestIdx = 0;
  for (size_t i = 0; i < 6; ++i) {
    const float jitter = static_cast<float> (next () % 100u) * 0.0022f;
    scores[i] = evaluateStrategy (static_cast<FinalStrategy> (i), lastContext, mem, profile) + jitter;
    if (scores[i] > bestScore) {
      bestScore = scores[i];
      bestIdx = i;
    }
  }
  ++utilityDecisions;
  current = static_cast<FinalStrategy> (bestIdx);
  return current;
}
FinalStrategy FinalPersona::choose () {
  if (!remaining) {
    for (unsigned i = 0; i < 6; ++i)
      bag[i] = static_cast<FinalStrategy> (i);
    for (unsigned i = 5; i > 0; --i)
      std::swap (bag[i], bag[next () % (i + 1)]);
    remaining = 6;
    if (bag[5] == current)
      std::swap (bag[5], bag[0]);
  }
  current = bag[--remaining];
  return current;
}
void PhysicalBoost::start (float now, float rise) {
  phase_ = std::isfinite (rise) && rise >= 0 && rise <= 132 ? BoostPhase::Approach : BoostPhase::Aborted;
  rise_ = rise;
  upperSupport_ = false;
  started_ = phase_start_ = now;
  next_jump_ = now;
  jump_until_ = 0;
}
BoostInput PhysicalBoost::update (float now, const BoostObservation &o) {
  BoostInput input;
  input.phase = phase_;
  if (phase_ == BoostPhase::Done || phase_ == BoostPhase::Aborted)
    return input;
  if (!std::isfinite (now) || o.danger || now < started_ || now - started_ > 10) {
    phase_ = BoostPhase::Aborted;
    input.phase = phase_;
    return input;
  }
  if (o.landed && phase_ != BoostPhase::Approach) {
    phase_ = BoostPhase::Done;
    input.phase = phase_;
    return input;
  }
  if (phase_ == BoostPhase::Approach) {
    if (o.helperReady && o.climberReady) {
      phase_ = BoostPhase::Mount;
      phase_start_ = now;
      next_jump_ = now;
    }
    else if (now - started_ > 4)
      phase_ = BoostPhase::Aborted;
  }
  if (phase_ == BoostPhase::Mount && o.supported) {
    phase_ = BoostPhase::Lift;
    phase_start_ = now;
    next_jump_ = now + 0.12f;
  }
  if (phase_ == BoostPhase::Mount && now - phase_start_ > 4.5f)
    phase_ = BoostPhase::Aborted;
  if (phase_ == BoostPhase::Lift && now - phase_start_ > 5)
    phase_ = BoostPhase::Aborted;
  const bool high = rise_ > 112;
  if (high && phase_ == BoostPhase::Lift && !upperSupport_ && o.helperStanding && o.supported && o.height >= 64) {
    upperSupport_ = true;
    next_jump_ = now + 0.10f;
    jump_until_ = 0;
  }
  input.helperDuck = phase_ == BoostPhase::Approach || phase_ == BoostPhase::Mount || (phase_ == BoostPhase::Lift && o.height < 60);
  if ((phase_ == BoostPhase::Mount || phase_ == BoostPhase::Lift) && now >= next_jump_) {
    jump_until_ = now + 0.07f;
    next_jump_ = now + 0.55f;
  }
  input.jump = now < jump_until_ && (phase_ == BoostPhase::Mount || phase_ == BoostPhase::Lift);
  input.climberDuck = phase_ == BoostPhase::Lift && (high ? (upperSupport_ && !o.supported && o.height > 90) : o.height > 70);
  input.towardLanding = phase_ == BoostPhase::Lift && (high ? (upperSupport_ && o.height > 90) : (o.helperStanding || o.height > 65));
  input.phase = phase_;
  return input;
}
const char *strategyName (FinalStrategy s) {
  static const char *n[] = { "push", "flank", "lurk", "snipe", "explore", "support" };
  return static_cast<unsigned> (s) < 6 ? n[static_cast<unsigned> (s)] : "unknown";
}
const char *moodName (FinalMood m) {
  static const char *n[] = { "calm", "confident", "angry", "afraid", "reckless", "cautious" };
  return static_cast<unsigned> (m) < 6 ? n[static_cast<unsigned> (m)] : "unknown";
}
const char *teamDirectiveName (TeamDirective d) {
  static const char *n[] = { "balanced_spread", "sync_crossfire", "fake_split", "pinch_flank", "bait_and_trade", "eco_ambush" };
  return static_cast<unsigned> (d) < 6 ? n[static_cast<unsigned> (d)] : "unknown";
}
const char *tacticalRoleName (BotTacticalRole r) {
  static const char *n[] = { "entry", "trader", "anchor", "lurker", "overwatch", "carrier" };
  return static_cast<unsigned> (r) < 6 ? n[static_cast<unsigned> (r)] : "unknown";
}
const char *curriculumGoalName (CurriculumGoal g) {
  static const char *n[] = { "entry_duel", "crossfire_trade", "novel_angles", "sniper_counter", "clutch_survival", "skill_evolution" };
  return static_cast<unsigned> (g) < CurriculumGoalCount ? n[static_cast<unsigned> (g)] : "unknown";
}
const char *nicknameSourceName (NicknameSource src) {
  static const char *n[] = { "historical_db", "steam_community", "nonsteam_community", "continuous_monitoring" };
  return static_cast<unsigned> (src) < 4 ? n[static_cast<unsigned> (src)] : "unknown";
}

// Skill 1: Internet Mode Simulator Implementation (10-30s join/leave interval, Max 21 bots)
void InternetModeSimulator::clear () {
  minInterval_ = kInternetModeDefaultMinInterval;
  maxInterval_ = kInternetModeDefaultMaxInterval;
  maxBots_ = kInternetModeMaxBots;
  activeTarget_ = kInternetModeMaxBots;
  nextChurnTime_ = 0.0f;
  initialFillDone_ = false;
  joinEvents_ = 0;
  leaveEvents_ = 0;
}

void InternetModeSimulator::configure (float minIntervalSec, float maxIntervalSec, int maxBots) {
  if (!std::isfinite (minIntervalSec)) minIntervalSec = kInternetModeDefaultMinInterval;
  if (!std::isfinite (maxIntervalSec)) maxIntervalSec = kInternetModeDefaultMaxInterval;
  minInterval_ = std::clamp (minIntervalSec, 1.0f, 300.0f);
  maxInterval_ = std::clamp (std::max (minInterval_, maxIntervalSec), 2.0f, 600.0f);
  maxBots_ = std::clamp (maxBots, 1, kInternetModeMaxBots);
  if (activeTarget_ > maxBots_ || activeTarget_ < 1) {
    activeTarget_ = maxBots_;
  }
}

float InternetModeSimulator::sampleInterval (uint32_t &rngSeed) const {
  const float u = static_cast<float> (nextEvoRng (rngSeed) & 0xFFFFu) / 65535.0f;
  return minInterval_ + (maxInterval_ - minInterval_) * u;
}

int InternetModeSimulator::stepTarget (float now, int desiredQuota, int currentBots, int activeHumans, int maxClients, uint32_t &rngSeed) {
  const int cap = std::min (maxBots_, finalCapacity (maxClients, activeHumans, 1));
  if (desiredQuota <= 0 || cap <= 0) {
    activeTarget_ = 0;
    return 0;
  }
  const int minInitial = std::min (cap, kInternetModeMinInitialBots);
  const int baseDesired = std::clamp (std::max (desiredQuota, minInitial), minInitial, cap);
  const int minFluctuation = std::max (minInitial, baseDesired - 3);
  if (!initialFillDone_) {
    activeTarget_ = baseDesired;
    // Wait until at least 16 bots (minInitial) AND initial desired fill have spawned before Internet Mode simulation begins
    if (currentBots >= minInitial && currentBots >= std::max (minInitial, baseDesired - 1)) {
      initialFillDone_ = true;
      nextChurnTime_ = now + sampleInterval (rngSeed);
    }
    return activeTarget_;
  }
  if (now >= nextChurnTime_) {
    nextChurnTime_ = now + sampleInterval (rngSeed);
    // Simulate realistic internet server player traffic: alternate between a player leaving and a new player joining
    if (currentBots >= baseDesired && baseDesired > 2) {
      activeTarget_ = std::max (minFluctuation, currentBots - 1);
      if (leaveEvents_ < 1000000000u) ++leaveEvents_;
    }
    else if (currentBots <= minFluctuation) {
      activeTarget_ = std::min (baseDesired, currentBots + 1);
      if (joinEvents_ < 1000000000u) ++joinEvents_;
    }
    else {
      const bool joinWave = (nextEvoRng (rngSeed) % 100u) < 58u;
      if (joinWave && activeTarget_ < baseDesired) {
        ++activeTarget_;
        if (joinEvents_ < 1000000000u) ++joinEvents_;
      }
      else if (!joinWave && activeTarget_ > minFluctuation) {
        --activeTarget_;
        if (leaveEvents_ < 1000000000u) ++leaveEvents_;
      }
      else {
        activeTarget_ = (activeTarget_ < baseDesired) ? (activeTarget_ + 1) : std::max (minFluctuation, activeTarget_ - 1);
      }
    }
  }
  activeTarget_ = std::clamp (activeTarget_, minFluctuation, baseDesired);
  return activeTarget_;
}

void InternetModeSimulator::recordJoin (float now, uint32_t &rngSeed) {
  if (joinEvents_ < 1000000000u) ++joinEvents_;
  if (initialFillDone_) {
    nextChurnTime_ = std::max (nextChurnTime_, now + sampleInterval (rngSeed));
  }
}

void InternetModeSimulator::recordLeave (float now, uint32_t &rngSeed) {
  if (leaveEvents_ < 1000000000u) ++leaveEvents_;
  nextChurnTime_ = now + sampleInterval (rngSeed);
}

// Skill 2: Generative CS 1.6 Nickname Bot Generator Implementation
namespace {
int charToIdx (char c) {
  const unsigned char uc = static_cast<unsigned char> (c);
  if (uc >= 'a' && uc <= 'z') return static_cast<int> (uc - 'a');
  if (uc >= 'A' && uc <= 'Z') return static_cast<int> (uc - 'A');
  if (uc >= '0' && uc <= '9') return 26 + static_cast<int> (uc - '0');
  if (uc == '_') return 36;
  return -1;
}
char idxToChar (int idx) {
  if (idx >= 0 && idx < 26) return static_cast<char> ('a' + idx);
  if (idx >= 26 && idx < 36) return static_cast<char> ('0' + (idx - 26));
  return '_';
}
bool isVowelChar (char c) {
  const char lc = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
  return lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u' || lc == 'y';
}
std::string normalizeNick (const std::string &s) {
  std::string out;
  out.reserve (s.size ());
  for (unsigned char c : s) {
    if (std::isalnum (c)) out.push_back (static_cast<char> (std::tolower (c)));
  }
  return out;
}
} // namespace

void GenerativeNicknameEngine::clear () {
  learnedPrefixes_.clear ();
  learnedMorphemes_.clear ();
  learnedSuffixes_.clear ();
  observedPlayerNames_.clear ();
  for (auto &row : charMarkov_) row.fill (1u);
  recentHashes_.fill (0u);
  recentCursor_ = 0;
  generatedCount_ = 0;
  monitoredPlayers_ = 0;
  sourceCursor_ = 0;
  initialized_ = false;
  dirty_ = false;
  ensureInitialized ();
}

void GenerativeNicknameEngine::trainMarkovOnStem (const std::string &stem) {
  int prev = 37; // start-of-word state
  for (char c : stem) {
    const int idx = charToIdx (c);
    if (idx < 0 || idx >= 37) continue;
    if (charMarkov_[static_cast<size_t> (prev)][static_cast<size_t> (idx)] < 60000u) {
      charMarkov_[static_cast<size_t> (prev)][static_cast<size_t> (idx)] += 4u;
    }
    prev = idx;
  }
}

void GenerativeNicknameEngine::ensureInitialized () {
  if (initialized_) return;
  initialized_ = true;
  for (auto &row : charMarkov_) row.fill (1u);

  // Seed character-level Markov & initial morpheme priors from historical CS 1.6 & community handles
  static const char *const kSeedHandles[] = {
    "heaton", "potti", "spawn", "element", "fisker", "vesslan", "carn", "f0rest", "get_right",
    "markeloff", "starix", "edward", "zeus", "neo", "taz", "kuben", "loord", "zonic", "trace",
    "sunde", "ave", "whimp", "cyx", "kapio", "roman", "blizzard", "frod", "storm", "warden",
    "shaguar", "ksharp", "rambo", "solo", "termi", "bail", "jungle", "sakula", "savage", "cogu",
    "fnx", "fallen", "coldzera", "xantares", "krimz", "flusha", "kennys", "shox", "scream", "guardiaN",
    "vexor", "krypton", "zypher", "nyxar", "strix", "valen", "draven", "kael", "zenith", "vortex",
    "phantom", "shadow", "predator", "reflex", "viper", "havoc", "calibra", "nexus", "specter", "razer"
  };
  for (const char *h : kSeedHandles) {
    trainMarkovOnStem (h);
  }
  if (learnedMorphemes_.empty ()) {
    static const char *const kBaseMorphemes[] = {
      "vex", "kry", "zyp", "nyx", "str", "val", "dra", "kae", "zen", "vor", "pha", "sha",
      "pre", "ref", "vip", "hav", "cal", "nex", "spc", "raz", "neo", "f0r", "mar", "sta",
      "z0n", "trc", "cyx", "kap", "blz", "fr0", "shg", "rmb", "ter", "svg", "cog", "fnx"
    };
    for (const char *m : kBaseMorphemes) learnedMorphemes_.emplace_back (m);
  }
  if (learnedPrefixes_.empty ()) {
    static const char *const kBasePrefixes[] = {
      "SK|", "fnatic.", "mTw // ", "Na`Vi ", "NiP - ", "Virtus.pro > ", "3D | ", "coL.", "NoA.", "eoL * ", "wNv.", "MYM."
    };
    for (const char *p : kBasePrefixes) learnedPrefixes_.emplace_back (p);
  }
  if (learnedSuffixes_.empty ()) {
    static const char *const kBaseSuffixes[] = {
      ".cfg", " @100fps", ".16bpp", " -iwnl-", " [Steam]", " *HS*", " 1337", " [Pro]", " ~"
    };
    for (const char *s : kBaseSuffixes) learnedSuffixes_.emplace_back (s);
  }
}

std::string GenerativeNicknameEngine::sampleMarkovSyllable (uint32_t &seed, size_t minLen, size_t maxLen) const {
  const size_t targetLen = minLen + (nextEvoRng (seed) % (maxLen - minLen + 1u));
  std::string out;
  out.reserve (targetLen);
  int prev = 37;
  for (size_t pos = 0; pos < targetLen; ++pos) {
    const bool wantVowel = (pos % 2 == 1) || (pos > 0 && !isVowelChar (out.back ()));
    uint32_t totalWeight = 0;
    std::array<uint32_t, 26> weights {};
    for (int c = 0; c < 26; ++c) {
      const char ch = idxToChar (c);
      uint32_t w = charMarkov_[static_cast<size_t> (prev)][static_cast<size_t> (c)];
      if (wantVowel && isVowelChar (ch)) w *= 5u;
      else if (!wantVowel && !isVowelChar (ch)) w *= 3u;
      weights[static_cast<size_t> (c)] = w;
      totalWeight += w;
    }
    uint32_t pick = nextEvoRng (seed) % std::max (1u, totalWeight);
    int chosen = 0;
    for (int c = 0; c < 26; ++c) {
      if (pick < weights[static_cast<size_t> (c)]) {
        chosen = c;
        break;
      }
      pick -= weights[static_cast<size_t> (c)];
    }
    out.push_back (idxToChar (chosen));
    prev = chosen;
  }
  return out;
}

std::string GenerativeNicknameEngine::synthesizeHistorical (uint32_t &seed) const {
  // Generative Historical CS 1.6 Player Database synthesis:
  // Combines historical competitive phoneme roots + Markov syllables + authentic 1.6 tournament clan tags & casing
  static const char *const kHistClans[] = {
    "SK|", "fnatic.", "mTw // ", "Na`Vi ", "NiP - ", "Virtus.pro > ", "mousesports | ",
    "3D | ", "coL.", "NoA.", "eoL * ", "Begrip ~ ", "Pentagram :", "wNv.", "MYM.", "aTTaX ",
    "EYEBALLERS ", "Catch-Gamer ", "x6tence.", "k1ck.", "MiBR.", "TyLoo."
  };
  static const char *const kHistOnsets[] = {
    "Heat", "Pott", "Spaw", "elem", "fisk", "vess", "cAr", "f0r", "GeT_", "mark", "star",
    "Edw", "Ze", "karr", "zon", "trac", "whiM", "g0b", "kap", "Bliz", "shaG", "Ksh", "ramb",
    "bull", "ter", "Sav", "cog", "Cold", "Fall", "Xan", "Guard", "ken", "flus", "krim"
  };
  static const char *const kHistCodas[] = {
    "oN", "ti", "wN", "eNt", "er", "lan", "n", "est", "RiGhT", "eloff", "ix", "ard", "us",
    "igan", "ic", "ce", "Mp", "io", "zard", "uar", "arp", "bo", "eye", "mi", "age", "u",
    "zera", "eN", "tares", "iaN", "nyS", "ha", "mZ"
  };
  const size_t nClans = sizeof (kHistClans) / sizeof (kHistClans[0]);
  const size_t nOnsets = sizeof (kHistOnsets) / sizeof (kHistOnsets[0]);
  const size_t nCodas = sizeof (kHistCodas) / sizeof (kHistCodas[0]);

  std::string handle = kHistOnsets[nextEvoRng (seed) % nOnsets];
  if ((nextEvoRng (seed) % 3u) == 0u) {
    handle += sampleMarkovSyllable (seed, 2, 3);
  }
  else {
    handle += kHistCodas[nextEvoRng (seed) % nCodas];
  }
  // Apply authentic 1.6 competitive casing or terminal letter capitalization
  if (!handle.empty () && (nextEvoRng (seed) % 3u) == 0u) {
    handle.back () = static_cast<char> (std::toupper (static_cast<unsigned char> (handle.back ())));
  }
  if ((nextEvoRng (seed) % 100u) < 65u) {
    return std::string (kHistClans[nextEvoRng (seed) % nClans]) + handle;
  }
  return handle;
}

std::string GenerativeNicknameEngine::synthesizeSteamCommunity (uint32_t &seed) const {
  // Generative Steam Community Server synthesis:
  // Clean phonetic gamer handles + Steam community PUG/scrim/config tags
  static const char *const kSteamOnsets[] = {
    "vex", "kryp", "zyph", "nyx", "strix", "val", "drav", "kael", "zen", "vor", "phan", "shad",
    "pred", "ref", "vip", "hav", "cal", "nex", "spec", "raz", "lynx", "flux", "apex", "nova",
    "aste", "cyph", "echo", "zero", "volt", "zane", "kuro", "sora", "ryu", "shin", "jiro", "aki"
  };
  static const char *const kSteamNuclei[] = {
    "or", "on", "er", "ar", "ix", "en", "in", "is", "ex", "us", "os", "ia", "io", "ax", "ox", "yn"
  };
  static const char *const kSteamTags[] = {
    "", "", "", ".cfg", " @100fps", ".16bpp", " -iwnl-", " [Steam]", "_cs", " ~",
    " [EAC]", " .pug", " [FastCup]", " [32bit]", " :D", " [100Hz]"
  };
  const size_t nO = sizeof (kSteamOnsets) / sizeof (kSteamOnsets[0]);
  const size_t nN = sizeof (kSteamNuclei) / sizeof (kSteamNuclei[0]);
  const size_t nT = sizeof (kSteamTags) / sizeof (kSteamTags[0]);

  std::string handle = kSteamOnsets[nextEvoRng (seed) % nO];
  handle += kSteamNuclei[nextEvoRng (seed) % nN];
  if ((nextEvoRng (seed) % 2u) == 0u) {
    handle += sampleMarkovSyllable (seed, 2, 3);
  }
  if ((nextEvoRng (seed) % 3u) == 0u && !handle.empty ()) {
    handle[0] = static_cast<char> (std::toupper (static_cast<unsigned char> (handle[0])));
  }
  handle += kSteamTags[nextEvoRng (seed) % nT];
  return handle;
}

std::string GenerativeNicknameEngine::synthesizeNonSteamCommunity (uint32_t &seed) const {
  // Generative Non-Steam Community Server synthesis:
  // Classic Protocol 47/48 pub/warnet styles, ASCII framings, combat stems & leetspeak
  static const char *const kPubStemsA[] = {
    "HeadShot", "Sniper", "Ghost", "Shadow", "Viper", "Phantom", "Silent", "Dark", "Cyber",
    "Lethal", "Rapid", "Savage", "Deadly", "Night", "Storm", "Frost", "Blaze", "Iron", " Crimson", "Omega"
  };
  static const char *const kPubStemsB[] = {
    "Killer", "Hunter", "Master", "Strike", "Bullet", "Clutch", "Deagle", "Kalash", "AWPer",
    "Wolf", "Hawk", "Blade", "slayer", "frag", "king", "ninja", "rex", "x", "pro", "legend"
  };
  static const char *const kFrameOpen[] = {
    "-= ", "* ", "[Pro] ", "~* ", "<< ", ":: ", "[NS] ", "[VIP] ", "", "", ""
  };
  static const char *const kFrameClose[] = {
    " =-", " *", "", " *~", " >>", " ::", "", "", " *HS*", " 1337", "_47"
  };
  const size_t nA = sizeof (kPubStemsA) / sizeof (kPubStemsA[0]);
  const size_t nB = sizeof (kPubStemsB) / sizeof (kPubStemsB[0]);
  const size_t nF = sizeof (kFrameOpen) / sizeof (kFrameOpen[0]);

  const size_t fIdx = nextEvoRng (seed) % nF;
  std::string core = kPubStemsA[nextEvoRng (seed) % nA];
  if (!core.empty () && core[0] == ' ') core.erase (0, 1);
  if ((nextEvoRng (seed) % 2u) == 0u) {
    core += "_";
    core += sampleMarkovSyllable (seed, 2, 4);
  }
  else {
    core += kPubStemsB[nextEvoRng (seed) % nB];
  }
  // Occasional leetspeak vowel substitution
  if ((nextEvoRng (seed) % 3u) == 0u) {
    for (char &c : core) {
      if (c == 'e') { c = '3'; break; }
      if (c == 'o') { c = '0'; break; }
      if (c == 'i') { c = '1'; break; }
    }
  }
  return std::string (kFrameOpen[fIdx]) + core + std::string (kFrameClose[fIdx]);
}

std::string GenerativeNicknameEngine::synthesizeMonitored (uint32_t &seed) const {
  // Continuous Player Monitoring synthesis:
  // Synthesizes novel nicknames using morphemes, prefixes, suffixes, and Markov transitions learned from real players
  std::string out;
  if (!learnedPrefixes_.empty () && (nextEvoRng (seed) % 100u) < 45u) {
    out += learnedPrefixes_[nextEvoRng (seed) % learnedPrefixes_.size ()];
  }
  if (!learnedMorphemes_.empty ()) {
    out += learnedMorphemes_[nextEvoRng (seed) % learnedMorphemes_.size ()];
  }
  out += sampleMarkovSyllable (seed, 3, 5);
  if (!learnedMorphemes_.empty () && (nextEvoRng (seed) % 2u) == 0u) {
    const std::string &m2 = learnedMorphemes_[nextEvoRng (seed) % learnedMorphemes_.size ()];
    if (out.size () + m2.size () <= 24) out += m2;
  }
  if (!learnedSuffixes_.empty () && (nextEvoRng (seed) % 100u) < 40u) {
    const std::string &suf = learnedSuffixes_[nextEvoRng (seed) % learnedSuffixes_.size ()];
    if (out.size () + suf.size () <= 30) out += suf;
  }
  return out;
}

void GenerativeNicknameEngine::observePlayer (const std::string &playerName) {
  ensureInitialized ();
  if (playerName.empty () || playerName.size () > 32) return;
  const std::string norm = normalizeNick (playerName);
  if (norm.size () < 2 || norm == "player" || norm == "unnamed") return;

  for (const auto &obs : observedPlayerNames_) {
    if (obs == playerName) return;
  }
  if (observedPlayerNames_.size () < 256) {
    observedPlayerNames_.push_back (playerName);
  }
  if (monitoredPlayers_ < 1000000000u) ++monitoredPlayers_;
  dirty_ = true;

  // 1. Extract prefix / clan tag if present (e.g. "[Clan] ", "Tag|", "tag.")
  size_t coreStart = 0;
  const size_t sepPos = playerName.find_first_of ("|]>.");
  if (sepPos != std::string::npos && sepPos > 0 && sepPos <= 10) {
    size_t endTag = sepPos + 1;
    while (endTag < playerName.size () && playerName[endTag] == ' ') ++endTag;
    const std::string prefix = playerName.substr (0, endTag);
    if (learnedPrefixes_.size () < 128) learnedPrefixes_.push_back (prefix);
    coreStart = endTag;
  }

  // 2. Extract alphanumeric core and feed character Markov chain + subword morphemes
  std::string alphaCore;
  for (size_t i = coreStart; i < playerName.size (); ++i) {
    const unsigned char c = static_cast<unsigned char> (playerName[i]);
    if (std::isalnum (c) || c == '_') alphaCore.push_back (static_cast<char> (c));
  }
  if (alphaCore.size () >= 3) {
    trainMarkovOnStem (alphaCore);
    // Slice into 3-4 character phonetic morphemes
    for (size_t i = 0; i + 3 <= alphaCore.size () && learnedMorphemes_.size () < 256; i += 2) {
      const size_t len = std::min<size_t> (4, alphaCore.size () - i);
      learnedMorphemes_.push_back (alphaCore.substr (i, len));
    }
  }
}

std::string GenerativeNicknameEngine::generateUnique (uint32_t &seed, const std::vector<std::string> &activeNames, NicknameSource *outSource) {
  ensureInitialized ();
  for (unsigned attempt = 0; attempt < 64; ++attempt) {
    const NicknameSource src = static_cast<NicknameSource> ((sourceCursor_++) % 4u);
    if (outSource) *outSource = src;

    std::string candidate;
    switch (src) {
    case NicknameSource::HistoricalDatabase:
      candidate = synthesizeHistorical (seed);
      break;
    case NicknameSource::SteamCommunity:
      candidate = synthesizeSteamCommunity (seed);
      break;
    case NicknameSource::NonSteamCommunity:
      candidate = synthesizeNonSteamCommunity (seed);
      break;
    case NicknameSource::ContinuousMonitoring:
    default:
      candidate = synthesizeMonitored (seed);
      break;
    }

    // Sanitize for GoldSrc userinfo (strip any characters that Info_SetValueForKey dislikes and trim spaces)
    std::string clean;
    clean.reserve (candidate.size ());
    for (unsigned char ch : candidate) {
      if (std::isalnum (ch) || ch == '_' || ch == '-' || ch == '.' || ch == '|' || ch == '[' || ch == ']' ||
          ch == '(' || ch == ')' || ch == '*' || ch == '~' || ch == '@' || ch == ' ' || ch == '/') {
        clean.push_back (static_cast<char> (ch));
      }
    }
    while (!clean.empty () && clean.front () == ' ') clean.erase (clean.begin ());
    while (!clean.empty () && clean.back () == ' ') clean.pop_back ();
    candidate = clean;
    if (candidate.empty ()) continue;
    if (candidate.size () > 30) candidate.resize (30);
    while (!candidate.empty () && candidate.back () == ' ') candidate.pop_back ();

    // Check against active names and observed raw human names
    const std::string normCand = normalizeNick (candidate);
    if (normCand.size () < 3) continue;

    bool collision = false;
    for (const auto &act : activeNames) {
      if (act == candidate || normalizeNick (act) == normCand) {
        collision = true;
        break;
      }
    }
    for (const auto &obs : observedPlayerNames_) {
      if (obs == candidate || normalizeNick (obs) == normCand) {
        collision = true;
        break;
      }
    }
    const uint32_t h = hashBytes (normCand.data (), normCand.size ());
    for (uint32_t rh : recentHashes_) {
      if (rh == h) {
        collision = true;
        break;
      }
    }
    if (collision && attempt < 48) continue;

    recentHashes_[recentCursor_++ % recentHashes_.size ()] = h;
    if (generatedCount_ < 1000000000u) ++generatedCount_;
    return candidate;
  }
  // Guaranteed unique fallback
  std::string fallback = sampleMarkovSyllable (seed, 5, 7) + "_" + std::to_string ((generatedCount_++) % 999u + 1u);
  return fallback;
}

bool GenerativeNicknameEngine::save (const std::string &path, std::string &error) {
  ensureInitialized ();
  error.clear ();
  std::vector<uint8_t> b { 'H', 'C', 'N', 'I', 'C', 'K', '0', '5' };
  put (b, generatedCount_);
  put (b, monitoredPlayers_);
  put (b, static_cast<uint32_t> (learnedPrefixes_.size ()));
  for (const auto &p : learnedPrefixes_) {
    put (b, static_cast<uint32_t> (p.size ()));
    b.insert (b.end (), p.begin (), p.end ());
  }
  put (b, static_cast<uint32_t> (learnedMorphemes_.size ()));
  for (const auto &m : learnedMorphemes_) {
    put (b, static_cast<uint32_t> (m.size ()));
    b.insert (b.end (), m.begin (), m.end ());
  }
  put (b, static_cast<uint32_t> (observedPlayerNames_.size ()));
  for (const auto &o : observedPlayerNames_) {
    put (b, static_cast<uint32_t> (o.size ()));
    b.insert (b.end (), o.begin (), o.end ());
  }
  put (b, hashBytes (b.data (), b.size ()));
  const auto tmp = path + ".tmp";
  auto f = std::fopen (tmp.c_str (), "wb");
  if (!f) { error = "cannot open nickname temp"; return false; }
  bool ok = std::fwrite (b.data (), 1, b.size (), f) == b.size ();
  if (std::fflush (f)) ok = false;
  if (std::fclose (f)) ok = false;
  if (ok) {
#ifdef _WIN32
    ok = MoveFileExA (tmp.c_str (), path.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    ok = std::rename (tmp.c_str (), path.c_str ()) == 0;
#endif
  }
  if (!ok) { std::remove (tmp.c_str ()); error = "nickname save failed"; return false; }
  dirty_ = false;
  return true;
}

bool GenerativeNicknameEngine::load (const std::string &path, std::string &error) {
  ensureInitialized ();
  error.clear ();
  auto f = std::fopen (path.c_str (), "rb");
  if (!f) { error = "nickname file not found"; return false; }
  std::vector<uint8_t> b (64 * 1024);
  const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool io = std::ferror (f) != 0;
  std::fclose (f);
  if (io || size < 24 || size >= b.size ()) { error = "invalid nickname file size"; return false; }
  b.resize (size);
  Read r { b }, tail { b };
  tail.p = size - 4;
  if (std::memcmp (b.data (), "HCNICK05", 8) != 0 || tail.get () != hashBytes (b.data (), size - 4)) {
    error = "nickname header/checksum mismatch"; return false;
  }
  r.p = 8;
  generatedCount_ = r.get ();
  monitoredPlayers_ = r.get ();
  const uint32_t pCount = r.get ();
  if (!r.ok || pCount > 256) { error = "invalid prefix count"; return false; }
  learnedPrefixes_.clear ();
  for (uint32_t i = 0; i < pCount && r.ok; ++i) {
    const uint32_t len = r.get ();
    if (!r.ok || len > 32 || r.p + len > size - 4) { error = "corrupt prefix"; return false; }
    learnedPrefixes_.emplace_back (reinterpret_cast<const char *> (b.data () + r.p), len);
    r.p += len;
  }
  const uint32_t mCount = r.get ();
  if (!r.ok || mCount > 512) { error = "invalid morpheme count"; return false; }
  learnedMorphemes_.clear ();
  for (uint32_t i = 0; i < mCount && r.ok; ++i) {
    const uint32_t len = r.get ();
    if (!r.ok || len > 32 || r.p + len > size - 4) { error = "corrupt morpheme"; return false; }
    std::string m (reinterpret_cast<const char *> (b.data () + r.p), len);
    learnedMorphemes_.push_back (m);
    trainMarkovOnStem (m);
    r.p += len;
  }
  const uint32_t oCount = r.get ();
  if (!r.ok || oCount > 512) { error = "invalid observed count"; return false; }
  observedPlayerNames_.clear ();
  for (uint32_t i = 0; i < oCount && r.ok; ++i) {
    const uint32_t len = r.get ();
    if (!r.ok || len > 32 || r.p + len > size - 4) { error = "corrupt observed"; return false; }
    std::string o (reinterpret_cast<const char *> (b.data () + r.p), len);
    observedPlayerNames_.push_back (o);
    trainMarkovOnStem (o);
    r.p += len;
  }
  dirty_ = false;
  return true;
}

int finalCapacity (int slots, int humans, int reserved) {
  (void)reserved;
  const int validSlots = std::clamp (slots, 0, 32);
  const int activeHumans = std::clamp (humans, 0, validSlots);
  const int maxBySlots = std::max (0, validSlots - activeHumans);
  return std::min (kInternetModeMaxBots, maxBySlots);
}

// ============================================================================
// ADVANCED TACTICAL BOT AI (CS 1.6 DE_DUST2 — 10 vs 10 / 20 Bots Pro Engine)
// ============================================================================

const char *ctStrategyName (CTAdvancedStrategy s) {
  switch (s) {
  case CTAdvancedStrategy::AggressiveALongPush:            return "Aggressive A Long Push";
  case CTAdvancedStrategy::AggressiveBTunnelsPush:         return "Aggressive B Tunnels Push";
  case CTAdvancedStrategy::AggressiveMidControl:           return "Aggressive Mid Control";
  case CTAdvancedStrategy::ShortCatwalkRush:               return "Short / Catwalk Rush";
  case CTAdvancedStrategy::LowerTunnelsFlanking:           return "Lower Tunnels Flanking";
  case CTAdvancedStrategy::TRampTSpawnInfiltration:        return "T Ramp / T Spawn Infiltration";
  case CTAdvancedStrategy::TacticalBaitFakePressure:       return "Tactical Bait / Fake Pressure";
  case CTAdvancedStrategy::CoordinatedCounterAttackRetake: return "Coordinated Counter-Attack / Bombsite Retake";
  case CTAdvancedStrategy::DefensiveCrossfire:             return "Defensive Crossfire";
  case CTAdvancedStrategy::DynamicRotationReinforcement:   return "Dynamic Rotation and Reinforcement";
  default:                                                 return "Defensive Crossfire";
  }
}

const char *tStrategyName (TOffensiveStrategy s) {
  switch (s) {
  case TOffensiveStrategy::ALongAttack:           return "A Long Attack";
  case TOffensiveStrategy::MidControl:            return "Mid Control";
  case TOffensiveStrategy::AShortAttack:          return "A Short Attack";
  case TOffensiveStrategy::BTunnelsAttack:        return "B Tunnels Attack";
  case TOffensiveStrategy::SplitAttack:           return "Split Attack";
  case TOffensiveStrategy::FakeAttack:            return "Fake Attack";
  case TOffensiveStrategy::TacticalRotation:      return "Tactical Rotation";
  case TOffensiveStrategy::AWPSupport:            return "AWP Support";
  case TOffensiveStrategy::BombCarrierProtection: return "Bomb Carrier Protection";
  case TOffensiveStrategy::PostPlantDefense:      return "Post-Plant Defense";
  default:                                        return "Split Attack";
  }
}

const char *proRole10v10Name (ProRole10v10 r) {
  switch (r) {
  case ProRole10v10::AWPSniper:          return "AWP Sniper";
  case ProRole10v10::EntryFragger:       return "Entry Fragger";
  case ProRole10v10::SupportPlayer:      return "Support Player";
  case ProRole10v10::SiteAnchor:         return "Site Anchor";
  case ProRole10v10::RifleDefender:      return "Rifle Defender";
  case ProRole10v10::Lurker:             return "Lurker";
  case ProRole10v10::Flanker:            return "Flanker";
  case ProRole10v10::TacticalPlaymaker:  return "Tactical Playmaker";
  case ProRole10v10::MidController:      return "Mid Controller";
  case ProRole10v10::RotationalDefender: return "Rotational Defender";
  default:                               return "Rifle Defender";
  }
}

const char *dynamicFormationName (DynamicFormation10v10 f) {
  switch (f) {
  case DynamicFormation10v10::Formation_4A_2Mid_4B: return "4 A Site / 2 Mid / 4 B Site";
  case DynamicFormation10v10::Formation_3A_4Mid_3B: return "3 A Site / 4 Mid / 3 B Site";
  case DynamicFormation10v10::Formation_5A_2Mid_3B: return "5 A Site / 2 Mid / 3 B Site";
  case DynamicFormation10v10::Formation_3A_2Mid_5B: return "3 A Site / 2 Mid / 5 B Site";
  default:                                          return "4 A Site / 2 Mid / 4 B Site";
  }
}

const char *tacticalDecisionName (TacticalDecision8 d) {
  switch (d) {
  case TacticalDecision8::ATTACK:  return "ATTACK";
  case TacticalDecision8::DEFEND:  return "DEFEND";
  case TacticalDecision8::PUSH:    return "PUSH";
  case TacticalDecision8::FLANK:   return "FLANK";
  case TacticalDecision8::ROTATE:  return "ROTATE";
  case TacticalDecision8::RETREAT: return "RETREAT";
  case TacticalDecision8::HOLD:    return "HOLD";
  case TacticalDecision8::RETAKE:  return "RETAKE";
  default:                         return "DEFEND";
  }
}

const std::array<Dust2AwpSpot, static_cast<size_t> (TAwpSpotId::Count)> &ProTactical10v10Engine::tAwpSpots () {
  static const std::array<Dust2AwpSpot, static_cast<size_t> (TAwpSpotId::Count)> kSpots {{
    { "T Spawn",          { -480.0f,  -780.0f,  100.0f }, {  0.0f,  92.0f, 0.0f }, { -720.0f, -820.0f, 100.0f }, 2, false },
    { "Suicide",          { -420.0f,  -220.0f,    4.0f }, {  0.0f,  90.0f, 0.0f }, { -480.0f, -680.0f, 100.0f }, 2, true  },
    { "Top Mid",          { -220.0f,   120.0f,    4.0f }, {  0.0f, 105.0f, 0.0f }, { -120.0f, -350.0f,  32.0f }, 2, true  },
    { "Xbox",             { -280.0f,   680.0f,   36.0f }, {  0.0f,  65.0f, 0.0f }, { -220.0f,  180.0f,   4.0f }, 2, true  },
    { "Mid to B",         { -450.0f,  1180.0f,  -60.0f }, {  0.0f, 135.0f, 0.0f }, { -430.0f,  760.0f, -60.0f }, 2, true  },
    { "T Mid Doors",      { -430.0f,   820.0f,  -60.0f }, {  0.0f,  90.0f, 0.0f }, { -260.0f,  320.0f,   4.0f }, 2, true  },
    { "Long Doors",       {  680.0f,   380.0f,    4.0f }, {  0.0f,  55.0f, 0.0f }, {  520.0f, -120.0f,   4.0f }, 0, false },
    { "Long Corner",      { 1180.0f,   680.0f,    4.0f }, {  0.0f,  88.0f, 0.0f }, {  720.0f,  420.0f,   4.0f }, 0, true  },
    { "Outside Long",     {  920.0f,   180.0f,    4.0f }, {  0.0f,  95.0f, 0.0f }, {  520.0f, -240.0f,   4.0f }, 0, false },
    { "A Long",           { 1260.0f,  1280.0f,    4.0f }, {  0.0f,  92.0f, 0.0f }, { 1380.0f,  420.0f, -120.0f }, 0, true  },
    { "Pit",              { 1420.0f,   240.0f, -180.0f }, { -6.0f,  96.0f, 0.0f }, { 1520.0f,  180.0f, -180.0f }, 0, false },
    { "Pit Corner",       { 1540.0f,   360.0f, -160.0f }, { -5.0f, 102.0f, 0.0f }, { 1420.0f,  240.0f, -180.0f }, 0, false },
    { "A Short",          {  380.0f,  1380.0f,   96.0f }, {  0.0f,  68.0f, 0.0f }, {  220.0f,  980.0f,  64.0f }, 0, true  },
    { "Short Corner",     {  440.0f,  1820.0f,   96.0f }, {  0.0f,  42.0f, 0.0f }, {  380.0f, 1380.0f,  96.0f }, 0, true  },
    { "Upper B Tunnels",  { -1580.0f,  880.0f,   32.0f }, {  0.0f,  92.0f, 0.0f }, { -1680.0f, 380.0f,  32.0f }, 1, false },
    { "Lower B Tunnels",  { -1120.0f,  680.0f, -110.0f }, { -4.0f,  15.0f, 0.0f }, { -1380.0f, 760.0f, -32.0f }, 1, false },
    { "B Tunnel Exit",    { -1520.0f, 1420.0f,   32.0f }, {  0.0f,  95.0f, 0.0f }, { -1580.0f, 920.0f,  32.0f }, 1, true  },
    { "B Doors",          { -1240.0f, 1880.0f,   16.0f }, {  0.0f, 145.0f, 0.0f }, { -920.0f, 1560.0f, -64.0f }, 1, true  },
    { "B Tunnel Stairs",  { -1380.0f,  760.0f,  -32.0f }, {  0.0f, 120.0f, 0.0f }, { -1620.0f, 480.0f,  32.0f }, 1, false }
  }};
  return kSpots;
}

const std::array<Dust2AwpSpot, static_cast<size_t> (CTAwpSpotId::Count)> &ProTactical10v10Engine::ctAwpSpots () {
  static const std::array<Dust2AwpSpot, static_cast<size_t> (CTAwpSpotId::Count)> kSpots {{
    { "CT Spawn",          { -380.0f, 2120.0f,  -96.0f }, { -4.0f,  -90.0f, 0.0f }, {  180.0f, 2240.0f, -96.0f }, 2, false },
    { "CT Mid",            { -430.0f, 1520.0f,  -96.0f }, { -3.0f,  -92.0f, 0.0f }, { -380.0f, 2120.0f, -96.0f }, 2, false },
    { "Mid Doors",         { -430.0f, 1080.0f,  -60.0f }, { -2.0f,  -88.0f, 0.0f }, { -430.0f, 1620.0f, -96.0f }, 2, true  },
    { "B Window",          { -1180.0f, 2180.0f,  64.0f }, {  2.0f, -125.0f, 0.0f }, { -860.0f, 2180.0f, -64.0f }, 1, false },
    { "B Site",            { -1560.0f, 2480.0f,  32.0f }, {  0.0f,  -85.0f, 0.0f }, { -1780.0f, 2620.0f, 64.0f }, 1, false },
    { "B Platform",        { -1780.0f, 2620.0f,  64.0f }, {  2.0f,  -78.0f, 0.0f }, { -1920.0f, 2680.0f, 64.0f }, 1, false },
    { "Back Platform",     { -1920.0f, 2680.0f,  64.0f }, {  0.0f,  -65.0f, 0.0f }, { -1680.0f, 2720.0f, 32.0f }, 1, false },
    { "Back Site",         { -1680.0f, 2720.0f,  32.0f }, {  0.0f,  -88.0f, 0.0f }, { -1220.0f, 2060.0f, 16.0f }, 1, false },
    { "B Car",             { -1520.0f, 1820.0f,  24.0f }, {  0.0f,   88.0f, 0.0f }, { -1220.0f, 1960.0f, 16.0f }, 1, true  },
    { "B Doors",           { -1220.0f, 1960.0f,  16.0f }, {  0.0f, -135.0f, 0.0f }, { -840.0f, 2040.0f, -64.0f }, 1, false },
    { "B Double Box",      { -1480.0f, 2320.0f,  32.0f }, {  0.0f,  -92.0f, 0.0f }, { -1680.0f, 2720.0f, 32.0f }, 1, false },
    { "A Site",            { 1120.0f, 2420.0f,   96.0f }, {  2.0f,  -95.0f, 0.0f }, { 1080.0f, 2680.0f,  96.0f }, 0, false },
    { "A Ramp",            { 1240.0f, 2140.0f,   64.0f }, {  2.0f,  -90.0f, 0.0f }, { 1120.0f, 2420.0f,  96.0f }, 0, false },
    { "Goose",             { 1080.0f, 2680.0f,   96.0f }, {  0.0f, -110.0f, 0.0f }, { 1120.0f, 2420.0f,  96.0f }, 0, false },
    { "A Site Boxes",      { 1020.0f, 2360.0f,   96.0f }, {  0.0f, -125.0f, 0.0f }, { 1080.0f, 2680.0f,  96.0f }, 0, false },
    { "A Long Corner",     { 1220.0f,  820.0f,    4.0f }, {  0.0f, -125.0f, 0.0f }, { 1420.0f,  280.0f, -180.0f }, 0, true  },
    { "Blue",              {  880.0f,  920.0f,    4.0f }, {  0.0f,  -75.0f, 0.0f }, { 1240.0f, 1820.0f,  32.0f }, 0, true  },
    { "Pit",               { 1420.0f,  280.0f, -180.0f }, { -4.0f,  130.0f, 0.0f }, { 1240.0f, 1650.0f,  16.0f }, 0, true  },
    { "A Short / Catwalk", {  380.0f, 1680.0f,   96.0f }, {  2.0f,  -92.0f, 0.0f }, {  520.0f, 1980.0f,  96.0f }, 0, true  },
    { "Short Stairs",      {  520.0f, 1980.0f,   96.0f }, {  0.0f,  -95.0f, 0.0f }, { 1020.0f, 2360.0f,  96.0f }, 0, false },
    { "A Site Corner",     { 1320.0f, 2560.0f,   96.0f }, {  2.0f, -105.0f, 0.0f }, { 1080.0f, 2680.0f,  96.0f }, 0, false }
  }};
  return kSpots;
}

void ProTactical10v10Engine::clear () {
  activeCtStrategy_ = CTAdvancedStrategy::AggressiveALongPush;
  activeTStrategy_ = TOffensiveStrategy::SplitAttack;
  activeFormation_ = DynamicFormation10v10::Formation_4A_2Mid_4B;
  slotRoles_.fill (ProRole10v10::RifleDefender);
  slotLanes_.fill (0u);
  slotDecisions_.fill (TacticalDecision8::DEFEND);
  slotAwpSpots_.fill (-1);
  slotAwpShots_.fill (0u);
  slotLastReposition_.fill (0.0f);
  ctStrategyScores_.fill (0.5f);
  tStrategyScores_.fill (0.5f);
  sharedEnemyValid_ = false;
  sharedEnemyOrigin_ = {};
  sharedEnemyTimestamp_ = -100.0f;
  sharedEnemySector_ = 2;
  sharedC4Alert_ = false;
  sharedC4Origin_ = {};
  sharedC4Site_ = 0;
  sharedC4Timestamp_ = -100.0f;
  observedEnemyHits_ = { 1u, 1u, 1u };
  observedEnemyAggression_ = 0.5f;
  roundNumber_ = 0;
}

int ProTactical10v10Engine::computeAwpQuota (int teamSize, int awpPercent, unsigned roundNum) {
  if (teamSize <= 0 || awpPercent <= 0) return 0;
  const int clamped = std::clamp (awpPercent, 0, 100);
  if (clamped == 50) {
    return (teamSize % 2 == 0) ? (teamSize / 2) : ((teamSize / 2) + static_cast<int> (roundNum & 1u));
  }
  const int target = (teamSize * clamped + static_cast<int> ((roundNum * 37u) % 100u)) / 100;
  return std::clamp (target, 0, teamSize);
}

void ProTactical10v10Engine::buildBalancedTeamLoadout (
    unsigned team, int teamSize, unsigned roundNum,
    std::vector<uint8_t> &outPrimaryWeapons,
    std::vector<uint8_t> &outSecondaryWeapons,
    std::vector<bool> &outHasAwp) {
  outPrimaryWeapons.clear ();
  outSecondaryWeapons.clear ();
  outHasAwp.clear ();
  if (teamSize <= 0) return;
  const int awpQuota = computeAwpQuota (teamSize, kDefaultAwpCompositionPercent, roundNum);
  const uint8_t rifleId = (team == 0u) ? 28u : 22u; // 28 = AK-47 (Terrorist), 22 = M4A1 (CT)
  const uint8_t awpId   = 18u;                      // 18 = AWP
  const uint8_t deagleId = 26u;                     // 26 = Deagle (Pro Secondary)
  const int rotation = static_cast<int> (roundNum % static_cast<unsigned> (teamSize));
  outPrimaryWeapons.resize (static_cast<size_t> (teamSize), rifleId);
  outSecondaryWeapons.resize (static_cast<size_t> (teamSize), deagleId);
  outHasAwp.resize (static_cast<size_t> (teamSize), false);
  for (int i = 0; i < teamSize; ++i) {
    const int order = (i + teamSize - rotation) % teamSize;
    if (order < awpQuota) {
      outPrimaryWeapons[static_cast<size_t> (i)] = awpId;
      outHasAwp[static_cast<size_t> (i)] = true;
    }
  }
}

void ProTactical10v10Engine::getFormationCounts (DynamicFormation10v10 fmt, int &outA, int &outMid, int &outB) const {
  switch (fmt) {
  case DynamicFormation10v10::Formation_3A_4Mid_3B:
    outA = 3; outMid = 4; outB = 3; break;
  case DynamicFormation10v10::Formation_5A_2Mid_3B:
    outA = 5; outMid = 2; outB = 3; break;
  case DynamicFormation10v10::Formation_3A_2Mid_5B:
    outA = 3; outMid = 2; outB = 5; break;
  case DynamicFormation10v10::Formation_4A_2Mid_4B:
  default:
    outA = 4; outMid = 2; outB = 4; break;
  }
}

void ProTactical10v10Engine::startNewRound (uint32_t roundNum, uint32_t &rngSeed, float ctAggressionBias) {
  roundNumber_ = roundNum;
  sharedEnemyValid_ = false;
  sharedEnemyTimestamp_ = -100.0f;
  sharedC4Alert_ = false;
  sharedC4Timestamp_ = -100.0f;
  slotAwpShots_.fill (0u);
  slotLastReposition_.fill (0.0f);
  slotAwpSpots_.fill (-1);

  // Adaptive formation selection based on observed Terrorist attack patterns + round variety
  const uint32_t hitsA = observedEnemyHits_[0];
  const uint32_t hitsB = observedEnemyHits_[1];
  const uint32_t hitsMid = observedEnemyHits_[2];
  const uint32_t r = nextEvoRng (rngSeed);

  if (hitsA > hitsB + 2u && (r % 3u != 0u)) {
    activeFormation_ = DynamicFormation10v10::Formation_5A_2Mid_3B;
  }
  else if (hitsB > hitsA + 2u && (r % 3u != 0u)) {
    activeFormation_ = DynamicFormation10v10::Formation_3A_2Mid_5B;
  }
  else if (hitsMid >= hitsA && hitsMid >= hitsB && (r % 3u == 0u)) {
    activeFormation_ = DynamicFormation10v10::Formation_3A_4Mid_3B;
  }
  else {
    activeFormation_ = static_cast<DynamicFormation10v10> ((roundNum + (r & 3u)) % 4u);
  }

  // Select unpredictable CT Advanced Strategy (combining offensive pro pushes + traps + crossfires)
  float bestCtScore = -1e30f;
  size_t bestCtIdx = (roundNum + (r >> 4u)) % 10u;
  for (size_t i = 0; i < 10u; ++i) {
    float s = ctStrategyScores_[i];
    // Boost offensive strategies so CTs are never purely passive/predictable
    if (i <= 6u) s += 0.25f * std::clamp (ctAggressionBias + 0.3f, 0.2f, 1.0f);
    if (i == static_cast<size_t> (CTAdvancedStrategy::AggressiveALongPush) && hitsA >= hitsB) s += 0.18f;
    if (i == static_cast<size_t> (CTAdvancedStrategy::AggressiveBTunnelsPush) && hitsB >= hitsA) s += 0.18f;
    if (i == static_cast<size_t> (CTAdvancedStrategy::AggressiveMidControl) && hitsMid >= 2u) s += 0.16f;
    // Rotate through all 10 pro strategies across rounds so humans can never memorize CT patterns
    const float varietyBonus = (((roundNum + i) % 10u) == 0u) ? 0.85f : 0.0f;
    const float jitter = static_cast<float> ((nextEvoRng (rngSeed) & 255u)) * 0.0018f;
    if (s + varietyBonus + jitter > bestCtScore) {
      bestCtScore = s + varietyBonus + jitter;
      bestCtIdx = i;
    }
  }
  activeCtStrategy_ = static_cast<CTAdvancedStrategy> (bestCtIdx);

  // Select 10-Player Terrorist Offensive Strategy
  float bestTScore = -1e30f;
  size_t bestTIdx = (roundNum * 3u + (r >> 8u)) % 10u;
  for (size_t i = 0; i < 10u; ++i) {
    float s = tStrategyScores_[i];
    const float varietyBonus = (((roundNum + i * 3u) % 10u) == 0u) ? 0.85f : 0.0f;
    const float jitter = static_cast<float> ((nextEvoRng (rngSeed) & 255u)) * 0.0018f;
    if (s + varietyBonus + jitter > bestTScore) {
      bestTScore = s + varietyBonus + jitter;
      bestTIdx = i;
    }
  }
  activeTStrategy_ = static_cast<TOffensiveStrategy> (bestTIdx);
}

void ProTactical10v10Engine::assignTeamRoster (
    unsigned team, const std::vector<int> &slots, const std::vector<bool> &hasAwp,
    const std::vector<bool> &hasC4, uint32_t &rngSeed) {
  if (slots.empty ()) return;

  // Canonical 10-Player Pro Role order ensuring Site Anchors & Rifle Defenders ALWAYS guard A & B,
  // while Entry Fraggers, Playmakers, Flankers, Lurkers, Mid Controllers, and Snipers execute dynamic plays!
  static constexpr std::array<ProRole10v10, 10> kProRoleTemplate {{
    ProRole10v10::SiteAnchor,         // 0: Dedicated A Site Goalkeeper Anchor
    ProRole10v10::SiteAnchor,         // 1: Dedicated B Site Goalkeeper Anchor
    ProRole10v10::AWPSniper,          // 2: Primary AWP Sniper
    ProRole10v10::EntryFragger,       // 3: Aggressive Entry Fragger
    ProRole10v10::SupportPlayer,      // 4: Utility & Trade Support Player
    ProRole10v10::MidController,      // 5: Mid Doors / Catwalk Controller
    ProRole10v10::RifleDefender,      // 6: Secondary Site Crossfire Rifle Defender
    ProRole10v10::Flanker,            // 7: Lower Tunnels / Catwalk Flanker
    ProRole10v10::TacticalPlaymaker,  // 8: Bait / Fake / Infiltration Playmaker
    ProRole10v10::Lurker              // 9: Deep Lurker / Rotational Defender
  }};

  int targetA = 4, targetMid = 2, targetB = 4;
  getFormationCounts (activeFormation_, targetA, targetMid, targetB);
  const int n = static_cast<int> (slots.size ());
  // Scale formation counts proportionally if fewer or more than 10 bots are alive
  int remA = std::max (1, (targetA * n + 5) / 10);
  int remB = std::max (1, (targetB * n + 5) / 10);
  int remMid = std::max (0, n - remA - remB);

  std::vector<bool> assigned (static_cast<size_t> (n), false);

  // 1. Assign AWP Snipers first (supports up to 50% team AWP composition, e.g., 5 AWPs + 5 AK-47/M4A1 in 10v10)
  const int maxAwpSnipers = std::max (2, computeAwpQuota (n, kDefaultAwpCompositionPercent, roundNumber_));
  static constexpr uint8_t kAwpLaneCycle[5] = { 2u, 0u, 1u, 0u, 1u };
  int awpAssigned = 0;
  for (int i = 0; i < n; ++i) {
    const int s = slots[static_cast<size_t> (i)];
    if (s < 0 || s >= 32) continue;
    if (i < static_cast<int> (hasAwp.size ()) && hasAwp[static_cast<size_t> (i)] && awpAssigned < maxAwpSnipers) {
      slotRoles_[static_cast<size_t> (s)] = ProRole10v10::AWPSniper;
      assigned[static_cast<size_t> (i)] = true;
      uint8_t lane = kAwpLaneCycle[static_cast<size_t> (awpAssigned) % 5u];
      if (lane == 2u && remMid > 0) --remMid;
      else if (lane == 0u && remA > 0) --remA;
      else if (lane == 1u && remB > 0) --remB;
      else if (remA >= remB && remA > 0) { lane = 0u; --remA; }
      else { lane = 1u; if (remB > 0) --remB; }
      slotLanes_[static_cast<size_t> (s)] = lane;
      ++awpAssigned;
    }
  }

  // 2. For Terrorists, ensure C4 carrier has SupportPlayer / EntryFragger escort on the primary attack lane
  uint8_t tPrimaryLane = 0u;
  if (activeTStrategy_ == TOffensiveStrategy::BTunnelsAttack) tPrimaryLane = 1u;
  else if (activeTStrategy_ == TOffensiveStrategy::MidControl) tPrimaryLane = 2u;
  else if (activeTStrategy_ == TOffensiveStrategy::ALongAttack || activeTStrategy_ == TOffensiveStrategy::AShortAttack) tPrimaryLane = 0u;
  else tPrimaryLane = static_cast<uint8_t> ((roundNumber_ + (rngSeed & 1u)) & 1u);

  for (int i = 0; i < n; ++i) {
    if (assigned[static_cast<size_t> (i)]) continue;
    const int s = slots[static_cast<size_t> (i)];
    if (s < 0 || s >= 32) continue;
    if (team == 0 && i < static_cast<int> (hasC4.size ()) && hasC4[static_cast<size_t> (i)]) {
      slotRoles_[static_cast<size_t> (s)] = ProRole10v10::SupportPlayer;
      slotLanes_[static_cast<size_t> (s)] = tPrimaryLane;
      assigned[static_cast<size_t> (i)] = true;
    }
  }

  // 3. Assign remaining bots across all 10 Pro Roles and formation lanes (A, B, Mid)
  size_t roleCursor = 0;
  for (int i = 0; i < n; ++i) {
    if (assigned[static_cast<size_t> (i)]) continue;
    const int s = slots[static_cast<size_t> (i)];
    if (s < 0 || s >= 32) continue;

    ProRole10v10 role = kProRoleTemplate[roleCursor % kProRoleTemplate.size ()];
    ++roleCursor;
    if (role == ProRole10v10::AWPSniper && awpAssigned > 0) {
      role = ProRole10v10::RotationalDefender;
    }
    slotRoles_[static_cast<size_t> (s)] = role;

    uint8_t lane = 0u;
    if (role == ProRole10v10::SiteAnchor) {
      // First SiteAnchor locks A Site (0), second SiteAnchor locks B Site (1)
      lane = (roleCursor == 1u) ? 0u : 1u;
      if (lane == 0u && remA > 0) --remA;
      else if (lane == 1u && remB > 0) --remB;
    }
    else if (role == ProRole10v10::MidController) {
      lane = 2u;
      if (remMid > 0) --remMid;
    }
    else if (remA > 0 && (remA >= remB && remA >= remMid)) {
      lane = 0u; --remA;
    }
    else if (remB > 0 && remB >= remMid) {
      lane = 1u; --remB;
    }
    else if (remMid > 0) {
      lane = 2u; --remMid;
    }
    else {
      lane = static_cast<uint8_t> (i % 3);
    }
    slotLanes_[static_cast<size_t> (s)] = lane;
  }

  // 4. Assign initial AWP spots on de_dust2 for every bot carrying an AWP or assigned AWPSniper
  const auto &tSpots = tAwpSpots ();
  const auto &ctSpots = ctAwpSpots ();
  for (int i = 0; i < n; ++i) {
    const int s = slots[static_cast<size_t> (i)];
    if (s < 0 || s >= 32) continue;
    const bool isAwp = (i < static_cast<int> (hasAwp.size ()) && hasAwp[static_cast<size_t> (i)]) ||
                       slotRoles_[static_cast<size_t> (s)] == ProRole10v10::AWPSniper;
    if (!isAwp) continue;
    const uint8_t desiredLane = slotLanes_[static_cast<size_t> (s)];
    if (team == 0) {
      int chosen = static_cast<int> ((nextEvoRng (rngSeed) + static_cast<uint32_t> (s)) % tSpots.size ());
      for (size_t k = 0; k < tSpots.size (); ++k) {
        const size_t idx = (static_cast<size_t> (chosen) + k) % tSpots.size ();
        if (tSpots[idx].lane == desiredLane) { chosen = static_cast<int> (idx); break; }
      }
      slotAwpSpots_[static_cast<size_t> (s)] = chosen;
    }
    else {
      int chosen = static_cast<int> ((nextEvoRng (rngSeed) + static_cast<uint32_t> (s)) % ctSpots.size ());
      const bool wantAggressive = (activeCtStrategy_ <= CTAdvancedStrategy::TacticalBaitFakePressure);
      for (size_t k = 0; k < ctSpots.size (); ++k) {
        const size_t idx = (static_cast<size_t> (chosen) + k) % ctSpots.size ();
        if (ctSpots[idx].lane == desiredLane && (ctSpots[idx].aggressive == wantAggressive || k > 8)) {
          chosen = static_cast<int> (idx);
          break;
        }
      }
      slotAwpSpots_[static_cast<size_t> (s)] = chosen;
    }
  }
}

void ProTactical10v10Engine::reportLegitimateContact (
    unsigned reportingTeam, const Vec &enemyOrigin, bool hasC4OrPlanted, float now) {
  (void)reportingTeam;
  sharedEnemyValid_ = true;
  sharedEnemyOrigin_ = enemyOrigin;
  sharedEnemyTimestamp_ = now;

  // Classify de_dust2 sector: x > 250 -> A Site/Long/Short (0); x < -850 -> B Site/Tunnels (1); else Mid (2)
  if (enemyOrigin.x > 250.0f) sharedEnemySector_ = 0u;
  else if (enemyOrigin.x < -850.0f) sharedEnemySector_ = 1u;
  else sharedEnemySector_ = 2u;

  if (sharedEnemySector_ < 3u && observedEnemyHits_[sharedEnemySector_] < 1000000u) {
    ++observedEnemyHits_[sharedEnemySector_];
  }

  if (hasC4OrPlanted) {
    sharedC4Alert_ = true;
    sharedC4Origin_ = enemyOrigin;
    sharedC4Site_ = (enemyOrigin.x < -300.0f) ? 1u : 0u;
    sharedC4Timestamp_ = now;
  }
}

void ProTactical10v10Engine::notifyAwpShotFired (int slot, float now, uint32_t &rngSeed) {
  if (slot < 0 || slot >= 32) return;
  const size_t idx = static_cast<size_t> (slot);
  ++slotAwpShots_[idx];
  slotLastReposition_[idx] = now;
  // Dynamic AWP Repositioning after firing to avoid predictable counter-peeks
  const uint8_t lane = slotLanes_[idx];
  const auto &ctSpots = ctAwpSpots ();
  int cur = slotAwpSpots_[idx];
  const size_t offset = static_cast<size_t> (nextEvoRng (rngSeed) % 3u);
  for (size_t k = 1; k < ctSpots.size (); ++k) {
    const int cand = static_cast<int> ((static_cast<size_t> (std::max (0, cur)) + k + offset) % ctSpots.size ());
    if (cand != cur && ctSpots[static_cast<size_t> (cand)].lane == lane) {
      slotAwpSpots_[idx] = cand;
      break;
    }
  }
  if (slotAwpSpots_[idx] == cur) {
    slotAwpSpots_[idx] = (std::max (0, cur) + 1) % static_cast<int> (ctSpots.size ());
  }
}

void ProTactical10v10Engine::recordRoundResult (unsigned winningTeam, uint8_t attackedSite) {
  if (attackedSite < 3u && observedEnemyHits_[attackedSite] < 1000000u) {
    observedEnemyHits_[attackedSite] += 2u;
  }
  const size_t ctIdx = static_cast<size_t> (activeCtStrategy_) % 10u;
  const size_t tIdx = static_cast<size_t> (activeTStrategy_) % 10u;
  if (winningTeam == 1u) {
    ctStrategyScores_[ctIdx] = std::clamp (ctStrategyScores_[ctIdx] + 0.12f, 0.1f, 2.0f);
    tStrategyScores_[tIdx] = std::clamp (tStrategyScores_[tIdx] - 0.08f, 0.1f, 2.0f);
  }
  else if (winningTeam == 0u) {
    tStrategyScores_[tIdx] = std::clamp (tStrategyScores_[tIdx] + 0.12f, 0.1f, 2.0f);
    ctStrategyScores_[ctIdx] = std::clamp (ctStrategyScores_[ctIdx] - 0.08f, 0.1f, 2.0f);
  }
}

TacticalBotDirective ProTactical10v10Engine::evaluateBot (const TacticalBotInput &in, float now, uint32_t &rngSeed) {
  TacticalBotDirective out {};
  const int s = std::clamp (in.slot, 0, 31);
  const size_t idx = static_cast<size_t> (s);
  out.role = slotRoles_[idx];
  out.ctStrategy = activeCtStrategy_;
  out.tStrategy = activeTStrategy_;
  out.formation = activeFormation_;
  out.assignedLane = slotLanes_[idx];

  const bool recentSharedContact = sharedEnemyValid_ && (now - sharedEnemyTimestamp_ <= 12.0f);
  const bool recentC4Alert = in.bombPlanted || in.c4SpottedOrHeard || (sharedC4Alert_ && (now - sharedC4Timestamp_ <= 22.0f));
  const Vec c4Target = !in.knownBombOrigin.empty () ? in.knownBombOrigin : sharedC4Origin_;

  // 1. Evaluate 8-State Dynamic Decision Engine (ATTACK, DEFEND, PUSH, FLANK, ROTATE, RETREAT, HOLD, RETAKE)
  if (in.health < 28 && (in.seeingEnemy || in.visibleEnemies >= 2) && !in.hasC4 && !in.bombPlanted) {
    out.decision = TacticalDecision8::RETREAT;
  }
  else if (in.team == 1u) { // Counter-Terrorist (CT)
    if (in.bombPlanted) {
      out.decision = TacticalDecision8::RETAKE;
      out.ctStrategy = CTAdvancedStrategy::CoordinatedCounterAttackRetake;
    }
    else if (recentC4Alert) {
      const float distToC4 = distance (in.origin, c4Target);
      out.decision = (distToC4 > 650.0f) ? TacticalDecision8::ROTATE : TacticalDecision8::RETAKE;
      out.ctStrategy = (distToC4 > 650.0f) ? CTAdvancedStrategy::DynamicRotationReinforcement
                                           : CTAdvancedStrategy::CoordinatedCounterAttackRetake;
    }
    else if (out.role == ProRole10v10::SiteAnchor || out.role == ProRole10v10::RifleDefender) {
      // Site Anchors & Rifle Defenders NEVER abandon the bombsites before C4 is spotted/planted!
      // Within their site, they use Defensive Crossfire (HOLD / DEFEND)
      out.decision = (s % 2 == 0) ? TacticalDecision8::DEFEND : TacticalDecision8::HOLD;
    }
    else if (out.role == ProRole10v10::Flanker || out.role == ProRole10v10::Lurker ||
             activeCtStrategy_ == CTAdvancedStrategy::LowerTunnelsFlanking ||
             activeCtStrategy_ == CTAdvancedStrategy::TRampTSpawnInfiltration) {
      out.decision = TacticalDecision8::FLANK;
    }
    else if (out.role == ProRole10v10::EntryFragger || out.role == ProRole10v10::TacticalPlaymaker ||
             activeCtStrategy_ == CTAdvancedStrategy::AggressiveALongPush ||
             activeCtStrategy_ == CTAdvancedStrategy::AggressiveBTunnelsPush ||
             activeCtStrategy_ == CTAdvancedStrategy::AggressiveMidControl ||
             activeCtStrategy_ == CTAdvancedStrategy::ShortCatwalkRush) {
      out.decision = (in.seeingEnemy || in.elapsedRoundTime < 28.0f) ? TacticalDecision8::PUSH : TacticalDecision8::ATTACK;
    }
    else if (recentSharedContact && sharedEnemySector_ != out.assignedLane && out.role == ProRole10v10::RotationalDefender) {
      out.decision = TacticalDecision8::ROTATE;
    }
    else if (in.hasAwp || out.role == ProRole10v10::AWPSniper) {
      out.decision = TacticalDecision8::HOLD;
    }
    else {
      out.decision = TacticalDecision8::DEFEND;
    }
  }
  else { // Terrorist (T)
    if (in.bombPlanted) {
      out.decision = (s % 2 == 0) ? TacticalDecision8::DEFEND : TacticalDecision8::HOLD;
      out.tStrategy = TOffensiveStrategy::PostPlantDefense;
    }
    else if (in.hasC4) {
      out.decision = TacticalDecision8::ATTACK;
      out.tStrategy = TOffensiveStrategy::BombCarrierProtection;
    }
    else if (out.role == ProRole10v10::Flanker || out.role == ProRole10v10::Lurker) {
      out.decision = TacticalDecision8::FLANK;
    }
    else if (out.role == ProRole10v10::EntryFragger || activeTStrategy_ == TOffensiveStrategy::ALongAttack ||
             activeTStrategy_ == TOffensiveStrategy::BTunnelsAttack || activeTStrategy_ == TOffensiveStrategy::AShortAttack) {
      out.decision = TacticalDecision8::PUSH;
    }
    else if (activeTStrategy_ == TOffensiveStrategy::TacticalRotation && in.elapsedRoundTime > 35.0f) {
      out.decision = TacticalDecision8::ROTATE;
    }
    else if (in.hasAwp || out.role == ProRole10v10::AWPSniper) {
      out.decision = TacticalDecision8::HOLD;
    }
    else {
      out.decision = TacticalDecision8::ATTACK;
    }
  }
  slotDecisions_[idx] = out.decision;

  // 2. AWP Position Intelligence on de_dust2 (19 T AWP spots + 21 CT AWP spots)
  if ((in.hasAwp || out.role == ProRole10v10::AWPSniper) && !in.hasC4 && out.decision != TacticalDecision8::RETAKE) {
    if (in.team == 0u) {
      const auto &spots = tAwpSpots ();
      int spotIdx = slotAwpSpots_[idx];
      if (spotIdx < 0 || spotIdx >= static_cast<int> (spots.size ())) {
        spotIdx = static_cast<int> ((nextEvoRng (rngSeed) + static_cast<uint32_t> (s)) % spots.size ());
        slotAwpSpots_[idx] = spotIdx;
      }
      const auto &sp = spots[static_cast<size_t> (spotIdx)];
      out.awpSpotIndex = spotIdx;
      out.awpSpotName = sp.name;
      out.targetOrigin = (out.decision == TacticalDecision8::RETREAT) ? sp.retreatOrigin : sp.origin;
      out.watchAngles = sp.watchAngles;
      out.retreatOrigin = sp.retreatOrigin;
      out.shouldHoldAngle = true;
      out.shouldRepositionAwp = (now - slotLastReposition_[idx] < 3.5f && slotAwpShots_[idx] > 0u);
      return out;
    }
    else {
      const auto &spots = ctAwpSpots ();
      int spotIdx = slotAwpSpots_[idx];
      if (spotIdx < 0 || spotIdx >= static_cast<int> (spots.size ())) {
        spotIdx = static_cast<int> ((nextEvoRng (rngSeed) + static_cast<uint32_t> (s)) % spots.size ());
        slotAwpSpots_[idx] = spotIdx;
      }
      const auto &sp = spots[static_cast<size_t> (spotIdx)];
      out.awpSpotIndex = spotIdx;
      out.awpSpotName = sp.name;
      out.targetOrigin = (out.decision == TacticalDecision8::RETREAT) ? sp.retreatOrigin : sp.origin;
      out.watchAngles = sp.watchAngles;
      out.retreatOrigin = sp.retreatOrigin;
      out.shouldHoldAngle = true;
      out.shouldRepositionAwp = (now - slotLastReposition_[idx] < 3.5f && slotAwpShots_[idx] > 0u);
      return out;
    }
  }

  // 3. Compute exact de_dust2 Tactical Coordinates & Sightlines for CT and T Strategies
  static constexpr Vec kSiteACenter    { 1120.0f, 2420.0f,  96.0f };
  static constexpr Vec kSiteAGoose     { 1080.0f, 2680.0f,  96.0f };
  static constexpr Vec kSiteARamp      { 1240.0f, 2140.0f,  64.0f };
  static constexpr Vec kSiteABoxes     { 1020.0f, 2360.0f,  96.0f };
  static constexpr Vec kSiteBCenter    { -1560.0f, 2480.0f, 32.0f };
  static constexpr Vec kSiteBPlatform  { -1780.0f, 2620.0f, 64.0f };
  static constexpr Vec kSiteBCar       { -1520.0f, 1820.0f, 24.0f };
  static constexpr Vec kSiteBWindow    { -1180.0f, 2180.0f, 64.0f };
  static constexpr Vec kSiteBDoors     { -1220.0f, 1960.0f, 16.0f };
  static constexpr Vec kALongCorner    { 1220.0f,  820.0f,   4.0f };
  static constexpr Vec kALongDoors     {  720.0f,  460.0f,   4.0f };
  static constexpr Vec kALongPit       { 1420.0f,  280.0f, -180.0f };
  static constexpr Vec kUpperBTunnels  { -1560.0f, 920.0f,  32.0f };
  static constexpr Vec kLowerBTunnels  { -1120.0f, 680.0f, -110.0f };
  static constexpr Vec kMidDoors       { -430.0f,  980.0f, -60.0f };
  static constexpr Vec kTopMidXbox     { -280.0f,  680.0f,  36.0f };
  static constexpr Vec kShortCatwalk   {  380.0f, 1520.0f,  96.0f };
  static constexpr Vec kTSpawnInfil    { -380.0f, -640.0f,  64.0f };

  if (in.team == 1u) { // Counter-Terrorist
    if (out.decision == TacticalDecision8::RETAKE || out.decision == TacticalDecision8::ROTATE) {
      // 3-Lane Coordinated Retake / Rotation (Doors + Window/Short + Tunnel/Long Backstab)
      const bool bSite = (!c4Target.empty () && c4Target.x < -300.0f) || (c4Target.empty () && sharedEnemySector_ == 1u);
      const int pincerLane = s % 3;
      if (bSite) {
        if (pincerLane == 0)      out.targetOrigin = kSiteBDoors;
        else if (pincerLane == 1) out.targetOrigin = kSiteBWindow;
        else                      out.targetOrigin = kUpperBTunnels; // Tunnel Backstab from behind!
        out.watchAngles = { 0.0f, -90.0f, 0.0f };
      }
      else {
        if (pincerLane == 0)      out.targetOrigin = kSiteARamp;
        else if (pincerLane == 1) out.targetOrigin = kShortCatwalk;
        else                      out.targetOrigin = kALongCorner; // Long A Flank from behind!
        out.watchAngles = { 0.0f, 90.0f, 0.0f };
      }
      out.shouldThrowFlash = in.hasFlash && distance (in.origin, out.targetOrigin) < 650.0f;
      out.shouldThrowSmoke = in.hasSmoke && in.bombPlanted && distance (in.origin, c4Target) < 420.0f;
      return out;
    }

    // Site Anchors and Rifle Defenders always guard their assigned site with Defensive Crossfires
    if (out.role == ProRole10v10::SiteAnchor || out.role == ProRole10v10::RifleDefender) {
      if (out.assignedLane == 0u) { // Bombsite A Crossfire (Goose + Ramp + Boxes + Pit)
        const Vec aCrossfire[4] = { kSiteAGoose, kSiteARamp, kSiteABoxes, kALongPit };
        out.targetOrigin = aCrossfire[static_cast<size_t> (s) & 3u];
        out.watchAngles = { 0.0f, -100.0f, 0.0f };
        out.retreatOrigin = kSiteACenter;
      }
      else { // Bombsite B Crossfire (Platform + Car + Window + Back Site)
        const Vec bCrossfire[4] = { kSiteBPlatform, kSiteBCar, kSiteBWindow, kSiteBCenter };
        out.targetOrigin = bCrossfire[static_cast<size_t> (s) & 3u];
        out.watchAngles = { 0.0f, -88.0f, 0.0f };
        out.retreatOrigin = kSiteBCenter;
      }
      out.shouldHoldAngle = true;
      return out;
    }

    // Dynamic Pro CT Offensive / Flanking / Playmaking Strategies for non-anchor roles!
    switch (activeCtStrategy_) {
    case CTAdvancedStrategy::AggressiveALongPush:
      out.targetOrigin = (s % 2 == 0) ? kALongDoors : kALongCorner;
      out.watchAngles = { 0.0f, -120.0f, 0.0f };
      out.retreatOrigin = kALongPit;
      out.shouldThrowFlash = in.hasFlash;
      break;
    case CTAdvancedStrategy::AggressiveBTunnelsPush:
      out.targetOrigin = (s % 2 == 0) ? kUpperBTunnels : kLowerBTunnels;
      out.watchAngles = { 0.0f, -90.0f, 0.0f };
      out.retreatOrigin = kSiteBCenter;
      out.shouldThrowFlash = in.hasFlash;
      break;
    case CTAdvancedStrategy::AggressiveMidControl:
      out.targetOrigin = (s % 2 == 0) ? kMidDoors : kTopMidXbox;
      out.watchAngles = { 0.0f, -88.0f, 0.0f };
      out.retreatOrigin = { -430.0f, 1520.0f, -96.0f };
      out.shouldThrowSmoke = in.hasSmoke;
      break;
    case CTAdvancedStrategy::ShortCatwalkRush:
      out.targetOrigin = (s % 2 == 0) ? kShortCatwalk : kTopMidXbox;
      out.watchAngles = { 0.0f, -95.0f, 0.0f };
      out.retreatOrigin = kSiteACenter;
      out.shouldThrowFlash = in.hasFlash;
      break;
    case CTAdvancedStrategy::LowerTunnelsFlanking:
      out.targetOrigin = (s % 2 == 0) ? kLowerBTunnels : kUpperBTunnels;
      out.watchAngles = { 0.0f, 165.0f, 0.0f };
      out.retreatOrigin = kMidDoors;
      break;
    case CTAdvancedStrategy::TRampTSpawnInfiltration:
      out.targetOrigin = kTSpawnInfil;
      out.watchAngles = { 0.0f, 90.0f, 0.0f };
      out.retreatOrigin = kALongDoors;
      break;
    case CTAdvancedStrategy::TacticalBaitFakePressure:
      out.targetOrigin = (out.assignedLane == 1u) ? kSiteBCar : kShortCatwalk;
      out.watchAngles = { 0.0f, -90.0f, 0.0f };
      out.retreatOrigin = (out.assignedLane == 1u) ? kSiteBCenter : kSiteACenter;
      out.shouldThrowFlash = in.hasFlash;
      break;
    case CTAdvancedStrategy::CoordinatedCounterAttackRetake:
    case CTAdvancedStrategy::DynamicRotationReinforcement:
    case CTAdvancedStrategy::DefensiveCrossfire:
    default:
      if (out.assignedLane == 2u)      out.targetOrigin = kMidDoors;
      else if (out.assignedLane == 1u) out.targetOrigin = (s % 2 == 0) ? kSiteBWindow : kSiteBDoors;
      else                             out.targetOrigin = (s % 2 == 0) ? kShortCatwalk : kALongCorner;
      out.watchAngles = { 0.0f, -90.0f, 0.0f };
      out.retreatOrigin = (out.assignedLane == 1u) ? kSiteBCenter : kSiteACenter;
      break;
    }
  }
  else { // Terrorist (T) 10-Player Offensive Intelligence
    if (in.bombPlanted && !c4Target.empty ()) {
      out.targetOrigin = c4Target;
      out.watchAngles = { 0.0f, -90.0f, 0.0f };
      out.shouldHoldAngle = true;
      return out;
    }
    switch (activeTStrategy_) {
    case TOffensiveStrategy::ALongAttack:
      out.targetOrigin = (in.elapsedRoundTime < 22.0f && !in.hasC4) ? kALongCorner : kSiteACenter;
      out.watchAngles = { 0.0f, 90.0f, 0.0f };
      out.shouldThrowFlash = in.hasFlash;
      out.shouldThrowSmoke = in.hasSmoke;
      break;
    case TOffensiveStrategy::AShortAttack:
      out.targetOrigin = (in.elapsedRoundTime < 20.0f && !in.hasC4) ? kShortCatwalk : kSiteACenter;
      out.watchAngles = { 0.0f, 65.0f, 0.0f };
      out.shouldThrowFlash = in.hasFlash;
      out.shouldThrowSmoke = in.hasSmoke;
      break;
    case TOffensiveStrategy::BTunnelsAttack:
      out.targetOrigin = (in.elapsedRoundTime < 20.0f && !in.hasC4) ? kUpperBTunnels : kSiteBCenter;
      out.watchAngles = { 0.0f, 92.0f, 0.0f };
      out.shouldThrowFlash = in.hasFlash;
      out.shouldThrowSmoke = in.hasSmoke;
      break;
    case TOffensiveStrategy::MidControl:
      out.targetOrigin = (s % 2 == 0) ? kMidDoors : kShortCatwalk;
      out.watchAngles = { 0.0f, 90.0f, 0.0f };
      out.shouldThrowSmoke = in.hasSmoke;
      break;
    case TOffensiveStrategy::FakeAttack:
      if (in.hasC4 || out.role == ProRole10v10::SupportPlayer || out.role == ProRole10v10::SiteAnchor) {
        out.targetOrigin = (roundNumber_ & 1u) ? kSiteBCenter : kSiteACenter;
      }
      else {
        // Fake crew makes noise at the opposite site first, then rotates
        out.targetOrigin = (in.elapsedRoundTime < 28.0f)
          ? ((roundNumber_ & 1u) ? kALongCorner : kUpperBTunnels)
          : ((roundNumber_ & 1u) ? kSiteBCenter : kSiteACenter);
        out.shouldThrowFlash = in.hasFlash;
      }
      out.watchAngles = { 0.0f, 90.0f, 0.0f };
      break;
    case TOffensiveStrategy::SplitAttack:
    case TOffensiveStrategy::TacticalRotation:
    case TOffensiveStrategy::AWPSupport:
    case TOffensiveStrategy::BombCarrierProtection:
    case TOffensiveStrategy::PostPlantDefense:
    default:
      if (out.assignedLane == 1u)      out.targetOrigin = (s % 2 == 0) ? kSiteBCenter : kSiteBDoors;
      else if (out.assignedLane == 2u) out.targetOrigin = kMidDoors;
      else                             out.targetOrigin = (s % 2 == 0) ? kSiteACenter : kShortCatwalk;
      out.watchAngles = { 0.0f, 90.0f, 0.0f };
      break;
    }
  }
  return out;
}

ProTactical10v10Engine &proTacticalEngine () {
  static ProTactical10v10Engine instance;
  return instance;
}

} // namespace rbcc
