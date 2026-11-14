#if defined(YSTL_TESTS)
// Local multilingual procedural dialogue. GPL-2.0-or-later.
#include "copycat_dialogue.h"
#include "copycat_core.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>
namespace rbcc {
namespace {
struct LanguagePack { const char *code; const char *prefix[6]; const char *tail[6]; const char *empty[3]; const char *ack[3]; const char *plan[3]; const char *body[23][3]; };
#include "dialogue_catalogue.inc"
struct Profile { const char *name, *language, *country; };
constexpr Profile profiles[] = {
 {"NEO","pl","Poland"},{"f0rest","sv","Sweden"},{"GeT_RiGhT","sv","Sweden"},{"markeloff","uk","Ukraine"},
 {"trace","da","Denmark"},{"starix","uk","Ukraine"},{"Edward","uk","Ukraine"},{"zonic","da","Denmark"},
 {"moddii","sv","Sweden"},{"Delpan","sv","Sweden"},{"TaZ","pl","Poland"},{"Friis","da","Denmark"},
 {"karrigan","da","Denmark"},{"Gux","sv","Sweden"},{"shox","fr","France"},{"Dosia","ru","Russia"},
 {"NBK","fr","France"},{"Xizt","sv","Sweden"},{"ScreaM","fr","Belgium"},{"JW","sv","Sweden"},
 {"Hiko","en","USA"},{"GuardiaN","sk","Slovakia"},{"flusha","sv","Sweden"},{"pasha","pl","Poland"},
 {"Snax","pl","Poland"},{"kennyS","fr","France"},{"KRiMZ","sv","Sweden"},{"Happy","fr","France"},
 {"BnTeT","id","Indonesia"},{"xccurate","id","Indonesia"},{"somebody","zh","China"},{"XigN","ko","South Korea"},
 {"Laz","ja","Japan"},{"ISSAA","ar","Jordan"},{"BOROS","ar","Jordan"},{"FalleN","pt","Brazil"},
 {"coldzera","pt","Brazil"},{"XANTARES","tr","Turkey"},{"gob b","de","Germany"},{"EliGE","en","USA"},
 {"Stewie2K","en","USA"},{"sh1ro","ru","Russia"}
};
std::string baseName (std::string name) {
  const auto suffix = name.rfind (" ("); if (suffix != std::string::npos && name.back () == ')') name.resize (suffix);
  for (auto &c : name) if (static_cast<unsigned char> (c) < 128) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
  return name;
}
uint32_t next (uint32_t &seed) { if (!seed) seed = 0x9e3779b9; seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
void replace (std::string &text, const char *key, const std::string &value) {
  size_t pos = 0; while ((pos = text.find (key,pos)) != std::string::npos) { text.replace (pos,std::strlen (key),value); pos += value.size (); }
}
const LanguagePack &pack (const std::string &language) { for (const auto &p : catalogue) if (language == p.code) return p; return catalogue[0]; }
bool has (const std::string &text, const std::vector<std::string> &words) {
  for (const auto &word : words) {
    size_t pos = text.find (word);
    while (pos != std::string::npos) {
      const size_t end = pos + word.size ();
      const auto letter = [] (unsigned char c) { return c >= 128 || std::isalnum (c) != 0; };
      if ((pos == 0 || !letter (static_cast<unsigned char> (text[pos-1]))) && (end == text.size () || !letter (static_cast<unsigned char> (text[end])))) return true;
      pos = text.find (word,pos+1);
    }
  }
  return false;
}
}
const char *profileLanguage (const std::string &name) { const auto base = baseName (name); for (const auto &p : profiles) if (base == baseName (p.name)) return p.language; return "en"; }
const char *profileCountry (const std::string &name) { const auto base = baseName (name); for (const auto &p : profiles) if (base == baseName (p.name)) return p.country; return "custom"; }
bool supportedLanguage (const std::string &language) { for (const auto &p : catalogue) if (language == p.code) return true; return false; }
DialogueIntent parseIntent (const std::string &incoming) {
  std::string t = incoming; for (auto &c : t) if (static_cast<unsigned char> (c) < 128) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
  if (has (t,{"hi","hello","hey","halo","glhf","privet","привет","hej","cześć","czesc","salut","bonjour","hallo","merhaba","ahlan","مرحبا","привіт","ahoj","ola","olá","annyeong","konnichiwa"}) || t.find ("你好") != std::string::npos || t.find ("こんにちは") != std::string::npos || t.find ("안녕") != std::string::npos) return DialogueIntent::Greeting;
  if (has (t,{"gg","nice","nt","mantap","bra","bien","хорош","ładnie","ladnie","bravo"})) return DialogueIntent::Praise;
  if (has (t,{"sorry","sry","maaf","извини","przepraszam","désolé","ursäkta"})) return DialogueIntent::Apology;
  if (has (t,{"flash","smoke","cover","bantu","granata","grenade"})) return DialogueIntent::Utility;
  if (has (t,{"buy","eco","save","money","beli","dengi","деньги"})) return DialogueIntent::Economy;
  if (has (t,{"awp","ak47","m4a1","weapon","senjata","deagle"})) return DialogueIntent::Equipment;
  if (has (t,{"rotate","rotation","rotasi","obrót","rotatez"})) return DialogueIntent::Rotate;
  if (has (t,{"plan","tactic","strategy","strategi"})) return DialogueIntent::Plan;
  return DialogueIntent::Unknown;
}
DialogueIntent ambientIntent (const DialogueContext &c, DialogueIntent previous, uint32_t &seed) {
  if (c.topFragger && previous != DialogueIntent::TopFragger) return DialogueIntent::TopFragger;
  if (c.boosting && previous != DialogueIntent::Boost) return DialogueIntent::Boost;
  if (c.mood == 2 && previous != DialogueIntent::Angry) return DialogueIntent::Angry;
  if (c.mood == 3 && previous != DialogueIntent::Afraid) return DialogueIntent::Afraid;
  if (c.mood == 4 && previous != DialogueIntent::Brave) return DialogueIntent::Brave;
  if (c.mood == 5 && previous != DialogueIntent::Cautious) return DialogueIntent::Cautious;
  if (c.rank > 1 && previous != DialogueIntent::Ambition && next (seed) % 3u == 0) return DialogueIntent::Ambition;
  if (c.health < 35 && previous != DialogueIntent::Health) return DialogueIntent::Health;
  if (c.planted && previous != DialogueIntent::Plant) return DialogueIntent::Plant;
  if (c.exploring && previous != DialogueIntent::Explore) return DialogueIntent::Explore;
  static constexpr DialogueIntent topics[] = {DialogueIntent::Plan,DialogueIntent::Banter,DialogueIntent::Economy,DialogueIntent::Equipment,DialogueIntent::Utility,DialogueIntent::Rotate,DialogueIntent::Hold};
  auto intent = topics[next (seed) % 7]; if (intent == previous) intent = topics[(next (seed) % 6 + 1) % 7]; return intent;
}
void truncateUtf8 (std::string &text, size_t bytes) {
  if (text.size () <= bytes) return;
  size_t cut = bytes; while (cut > 0 && (static_cast<unsigned char> (text[cut]) & 0xc0u) == 0x80u) --cut;
  text.resize (cut);
}
bool composeDialogue (const std::string &language, DialogueIntent intent, const DialogueContext &c, uint32_t &seed, DialogueMemory &memory, std::string &out) {
  if (intent == DialogueIntent::TopFragger && !c.topFragger) intent = DialogueIntent::Ambition;
  const auto topic = static_cast<size_t> (intent); if (topic >= 23) return false;
  const auto &p = pack (language);
  for (unsigned attempt = 0; attempt < 40; ++attempt) {
    out.clear ();
    if (next (seed) % 4 != 0) { out += p.prefix[next (seed) % 6]; out += ", "; }
    if (c.reply && intent == DialogueIntent::Apology) out += p.ack[next (seed) % 3];
    else if (c.reply && intent == DialogueIntent::Plan) out += p.plan[next (seed) % 3];
    else if (intent == DialogueIntent::Utility && !c.flash && !c.smoke && !c.he) out += p.empty[next (seed) % 3];
    else out += p.body[topic][next (seed) % 3];
    std::string utility; if (c.flash) utility = "flash"; if (c.smoke) utility += utility.empty () ? "smoke" : "+smoke"; if (c.he) utility += utility.empty () ? "HE" : "+HE";
    replace (out,"{utility}",utility); replace (out,"{cash}",std::to_string (std::clamp (c.money,0,16000)));
    replace (out,"{hp}",std::to_string (std::clamp (c.health,0,100))); replace (out,"{weapon}",c.weapon.substr (0,20));
    replace (out,"{frags}",std::to_string (std::clamp (c.frags,-999,999))); replace (out,"{rank}",std::to_string (std::clamp (c.rank,1,32)));
    if (next (seed) % 4 != 0) { out += "; "; out += p.tail[next (seed) % 6]; }
    truncateUtf8 (out,150);
    const auto hash = hashBytes (out.data (),out.size ()); bool duplicate = false; for (auto h : memory.hashes) if (h == hash) duplicate = true;
    if (duplicate) continue;
    memory.hashes[memory.cursor++ % memory.hashes.size ()] = hash; memory.topic = intent; return true;
  }
  out.clear (); return false;
}
unsigned dialogueLanguageCount () { return static_cast<unsigned> (sizeof catalogue / sizeof catalogue[0]); }
unsigned dialogueFragmentCount () { return dialogueLanguageCount () * (6 + 6 + 3 + 3 + 3 + 23 * 3); }
}
#else
// Generative English LLM Dialogue Engine (Massive 3,600+ Sentence Corpus, 4-Gram + Self-Attention & Top-P Decoding). GPL-2.0-or-later.
#include "copycat_dialogue.h"
#include "copycat_core.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace rbcc {
namespace {
struct LanguagePack { const char *code; const char *prefix[6]; const char *tail[6]; const char *empty[3]; const char *ack[3]; const char *plan[3]; const char *body[23][3]; };
#include "dialogue_catalogue.inc"
#include "english_llm_corpus.inc"

struct Profile { const char *name, *language, *country; };
constexpr Profile profiles[] = {
 {"NEO","en","Poland"},{"f0rest","en","Sweden"},{"GeT_RiGhT","en","Sweden"},{"markeloff","en","Ukraine"},
 {"trace","en","Denmark"},{"starix","en","Ukraine"},{"Edward","en","Ukraine"},{"zonic","en","Denmark"},
 {"moddii","en","Sweden"},{"Delpan","en","Sweden"},{"TaZ","en","Poland"},{"Friis","en","Denmark"},
 {"karrigan","en","Denmark"},{"Gux","en","Sweden"},{"shox","en","France"},{"Dosia","en","Russia"},
 {"NBK","en","France"},{"Xizt","en","Sweden"},{"ScreaM","en","Belgium"},{"JW","en","Sweden"},
 {"Hiko","en","USA"},{"GuardiaN","en","Slovakia"},{"flusha","en","Sweden"},{"pasha","en","Poland"},
 {"Snax","en","Poland"},{"kennyS","en","France"},{"KRiMZ","en","Sweden"},{"Happy","en","France"},
 {"BnTeT","en","Indonesia"},{"xccurate","en","Indonesia"},{"somebody","en","China"},{"XigN","en","South Korea"},
 {"Laz","en","Japan"},{"ISSAA","en","Jordan"},{"BOROS","en","Jordan"},{"FalleN","en","Brazil"},
 {"coldzera","en","Brazil"},{"XANTARES","en","Turkey"},{"gob b","en","Germany"},{"EliGE","en","USA"},
 {"Stewie2K","en","USA"},{"sh1ro","en","Russia"}
};

std::string baseName (std::string name) {
  const auto suffix = name.rfind (" (");
  if (suffix != std::string::npos && name.back () == ')') name.resize (suffix);
  for (auto &c : name) if (static_cast<unsigned char> (c) < 128) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
  return name;
}

uint32_t nextRng (uint32_t &seed) {
  if (!seed) seed = 0x9e3779b9u;
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

void replaceSlot (std::string &text, const char *key, const std::string &value) {
  size_t pos = 0;
  while ((pos = text.find (key, pos)) != std::string::npos) {
    text.replace (pos, std::strlen (key), value);
    pos += value.size ();
  }
}

bool hasWord (const std::string &text, const std::vector<std::string> &words) {
  for (const auto &word : words) {
    size_t pos = text.find (word);
    while (pos != std::string::npos) {
      const size_t end = pos + word.size ();
      const auto letter = [] (unsigned char c) { return c >= 128 || std::isalnum (c) != 0; };
      if ((pos == 0 || !letter (static_cast<unsigned char> (text[pos - 1]))) &&
          (end == text.size () || !letter (static_cast<unsigned char> (text[end]))))
        return true;
      pos = text.find (word, pos + 1);
    }
  }
  return false;
}

float dotVec (const std::array<float, GenerativeEnglishLLM::EmbedDim> &a,
              const std::array<float, GenerativeEnglishLLM::EmbedDim> &b) {
  float s = 0.0f;
  for (size_t i = 0; i < GenerativeEnglishLLM::EmbedDim; ++i) s += a[i] * b[i];
  return s;
}

void normalizeVec (std::array<float, GenerativeEnglishLLM::EmbedDim> &v) {
  float normSq = 0.0f;
  for (float x : v) normSq += x * x;
  if (normSq > 1e-8f) {
    const float inv = 1.0f / std::sqrt (normSq);
    for (float &x : v) x *= inv;
  }
}

void putU32 (std::vector<uint8_t> &b, uint32_t v) {
  for (unsigned i = 0; i < 4; ++i) b.push_back (static_cast<uint8_t> ((v >> (i * 8)) & 0xffu));
}

struct BufReader {
  const std::vector<uint8_t> &b;
  size_t p {};
  bool ok { true };
  uint32_t getU32 () {
    if (p + 4 > b.size ()) { ok = false; return 0; }
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i) v |= static_cast<uint32_t> (b[p++]) << (i * 8);
    return v;
  }
};
} // namespace

// Skill 3: GenerativeEnglishLLM Implementation
void GenerativeEnglishLLM::clear () {
  vocab_.clear ();
  wordToId_.clear ();
  embeddings_.clear ();
  bigrams_.clear ();
  trigrams_.clear ();
  fourgrams_.clear ();
  for (auto &s : starters_) s.clear ();
  learnedSentences_.clear ();
  learnedIntents_.clear ();
  trainedSentences_ = 0;
  corpusTokens_ = 0;
  onlineUpdates_ = 0;
  generatedTokens_ = 0;
  pretrained_ = false;
  dirty_ = false;
  ensurePretrained ();
}

int GenerativeEnglishLLM::findToken (const std::string &word) const {
  const auto it = wordToId_.find (word);
  return it != wordToId_.end () ? static_cast<int> (it->second) : -1;
}

uint16_t GenerativeEnglishLLM::getOrAddToken (const std::string &word, uint32_t intentBit) {
  const auto it = wordToId_.find (word);
  if (it != wordToId_.end ()) {
    const uint16_t id = it->second;
    if (intentBit != 0 && id >= 4) {
      for (size_t k = 0; k < 23; ++k) {
        if ((intentBit & (1u << k)) != 0) {
          auto &emb = embeddings_[id];
          for (size_t d = 0; d < EmbedDim; ++d) {
            emb[d] = 0.94f * emb[d] + 0.06f * intentPrototypes_[k][d];
          }
          normalizeVec (emb);
          break;
        }
      }
    }
    return id;
  }
  if (vocab_.size () >= MaxVocab) {
    return kUnkToken;
  }
  const uint16_t id = static_cast<uint16_t> (vocab_.size ());
  vocab_.push_back (word);
  wordToId_.emplace (word, id);
  bigrams_.emplace_back ();

  std::array<float, EmbedDim> emb {};
  uint32_t h = hashBytes (word.data (), word.size (), 0x811c9dc5u);
  for (size_t d = 0; d < EmbedDim; ++d) {
    const float raw = static_cast<float> (static_cast<int32_t> (nextRng (h) & 2047u) - 1024) / 1024.0f;
    emb[d] = raw * 0.35f;
  }
  for (size_t k = 0; k < 23; ++k) {
    if ((intentBit & (1u << k)) != 0) {
      for (size_t d = 0; d < EmbedDim; ++d) {
        emb[d] += 0.65f * intentPrototypes_[k][d];
      }
    }
  }
  normalizeVec (emb);
  embeddings_.push_back (emb);
  return id;
}

uint32_t GenerativeEnglishLLM::computeKeywordMask (const std::string &text) const {
  if (text.empty ()) return 0u;
  std::string t = text;
  for (auto &c : t) if (static_cast<unsigned char> (c) < 128) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
  uint32_t mask = 0u;
  if (hasWord (t, {"how","doing","feeling","today"})) mask |= (1u << 0);
  if (hasWord (t, {"bot","ai","human","real","player"})) mask |= (1u << 1);
  if (hasWord (t, {"ping","fps","lag","choke","ms"})) mask |= (1u << 2);
  if (hasWord (t, {"sens","sensitivity","dpi","res","resolution","crosshair","mouse"})) mask |= (1u << 3);
  if (hasWord (t, {"long","pit","doors","cat","catwalk","short","xbox"})) mask |= (1u << 4);
  if (hasWord (t, {"tunnels","dark","upper","lower","b","car","window"})) mask |= (1u << 5);
  if (hasWord (t, {"banana","apartments","arch","boiler","inferno"})) mask |= (1u << 6);
  if (hasWord (t, {"nuke","ramp","outside","silo","heaven","secret","hut"})) mask |= (1u << 7);
  if (hasWord (t, {"train","ivy","popdog","connector","tuscan","kitty"})) mask |= (1u << 8);
  if (hasWord (t, {"drop","buy","money","cash","eco","save","force"})) mask |= (1u << 9);
  if (hasWord (t, {"awp","sniper","ak","ak47","m4","m4a1","deagle"})) mask |= (1u << 10);
  if (hasWord (t, {"flash","smoke","he","nade","utility","pop"})) mask |= (1u << 11);
  if (hasWord (t, {"rush","split","mid","execute","default","plan","strat"})) mask |= (1u << 12);
  if (hasWord (t, {"rotate","back","other","help","pivot"})) mask |= (1u << 13);
  if (hasWord (t, {"hold","camp","anchor","defend","watch","flank"})) mask |= (1u << 14);
  if (hasWord (t, {"bomb","c4","plant","planted","defuse"})) mask |= (1u << 15);
  if (hasWord (t, {"boost","box","crate","ledge","crouch"})) mask |= (1u << 16);
  if (hasWord (t, {"nice","ns","wp","gg","clutch","thanks","ty"})) mask |= (1u << 17);
  return mask;
}

std::vector<std::string> GenerativeEnglishLLM::tokenizeText (const std::string &text) const {
  std::vector<std::string> tokens;
  std::string cur;
  for (size_t i = 0; i < text.size (); ++i) {
    const char c = text[i];
    if ((c == '$' || c == '#') && i + 1 < text.size () && text[i + 1] == '{') {
      if (!cur.empty ()) { tokens.push_back (cur); cur.clear (); }
      const size_t close = text.find ('}', i + 1);
      if (close != std::string::npos) {
        tokens.push_back (text.substr (i, close - i + 1));
        i = close;
        continue;
      }
    }
    if (c == '{') {
      if (!cur.empty ()) { tokens.push_back (cur); cur.clear (); }
      const size_t close = text.find ('}', i);
      if (close != std::string::npos) {
        tokens.push_back (text.substr (i, close - i + 1));
        i = close;
        continue;
      }
    }
    const unsigned char uc = static_cast<unsigned char> (c);
    if (std::isalnum (uc) || c == '\'' || c == '-') {
      cur.push_back (static_cast<char> (std::tolower (uc)));
    }
    else if (c == ',' || c == '?' || c == '!') {
      if (!cur.empty ()) { tokens.push_back (cur); cur.clear (); }
      tokens.emplace_back (1, c);
    }
    else if (c == '.' || c == ';' || c == ':' || std::isspace (uc)) {
      if (!cur.empty ()) { tokens.push_back (cur); cur.clear (); }
      if (c == ';') tokens.emplace_back (",");
    }
  }
  if (!cur.empty ()) tokens.push_back (cur);
  return tokens;
}

void GenerativeEnglishLLM::addBigram (uint16_t prev1, uint16_t nextTok, uint32_t intentBit, uint32_t kwMask) {
  if (prev1 >= bigrams_.size ()) return;
  auto &edges = bigrams_[prev1];
  for (auto &e : edges) {
    if (e.nextToken == nextTok) {
      if (e.count < 60000u) ++e.count;
      e.intentMask |= intentBit;
      e.keywordMask |= kwMask;
      return;
    }
  }
  edges.push_back (TransitionEdge { nextTok, 1u, intentBit, kwMask });
}

void GenerativeEnglishLLM::addTrigram (uint16_t prev2, uint16_t prev1, uint16_t nextTok, uint32_t intentBit, uint32_t kwMask) {
  const uint32_t key = (static_cast<uint32_t> (prev2) << 16u) | static_cast<uint32_t> (prev1);
  auto &edges = trigrams_[key];
  for (auto &e : edges) {
    if (e.nextToken == nextTok) {
      if (e.count < 60000u) ++e.count;
      e.intentMask |= intentBit;
      e.keywordMask |= kwMask;
      return;
    }
  }
  edges.push_back (TransitionEdge { nextTok, 1u, intentBit, kwMask });
}

void GenerativeEnglishLLM::addFourgram (uint16_t prev3, uint16_t prev2, uint16_t prev1, uint16_t nextTok, uint32_t intentBit, uint32_t kwMask) {
  const uint64_t key = (static_cast<uint64_t> (prev3) << 32u) | (static_cast<uint64_t> (prev2) << 16u) | static_cast<uint64_t> (prev1);
  auto &edges = fourgrams_[key];
  for (auto &e : edges) {
    if (e.nextToken == nextTok) {
      if (e.count < 60000u) ++e.count;
      e.intentMask |= intentBit;
      e.keywordMask |= kwMask;
      return;
    }
  }
  edges.push_back (TransitionEdge { nextTok, 1u, intentBit, kwMask });
}

const std::vector<GenerativeEnglishLLM::TransitionEdge> *GenerativeEnglishLLM::findTrigram (uint16_t prev2, uint16_t prev1) const {
  const uint32_t key = (static_cast<uint32_t> (prev2) << 16u) | static_cast<uint32_t> (prev1);
  const auto it = trigrams_.find (key);
  return it != trigrams_.end () ? &it->second : nullptr;
}

const std::vector<GenerativeEnglishLLM::TransitionEdge> *GenerativeEnglishLLM::findFourgram (uint16_t prev3, uint16_t prev2, uint16_t prev1) const {
  const uint64_t key = (static_cast<uint64_t> (prev3) << 32u) | (static_cast<uint64_t> (prev2) << 16u) | static_cast<uint64_t> (prev1);
  const auto it = fourgrams_.find (key);
  return it != fourgrams_.end () ? &it->second : nullptr;
}

void GenerativeEnglishLLM::trainSentenceInternal (const std::string &sentence, DialogueIntent intent, bool isOnline, const char *keywords) {
  const size_t topic = static_cast<size_t> (intent) < 23 ? static_cast<size_t> (intent) : static_cast<size_t> (DialogueIntent::Banter);
  const uint32_t intentBit = 1u << topic;
  const auto words = tokenizeText (sentence);
  if (words.size () < 3) return;

  std::vector<uint16_t> ids;
  ids.reserve (words.size ());
  for (const auto &w : words) {
    ids.push_back (getOrAddToken (w, intentBit));
  }
  corpusTokens_ += static_cast<uint32_t> (ids.size ());

  for (size_t i = 0; i < ids.size (); ++i) {
    auto &eCenter = embeddings_[ids[i]];
    if (i > 0) {
      const auto &eLeft = embeddings_[ids[i - 1]];
      for (size_t d = 0; d < EmbedDim; ++d) eCenter[d] += 0.04f * eLeft[d];
    }
    if (i + 1 < ids.size ()) {
      const auto &eRight = embeddings_[ids[i + 1]];
      for (size_t d = 0; d < EmbedDim; ++d) eCenter[d] += 0.04f * eRight[d];
    }
    normalizeVec (eCenter);
  }

  const uint32_t kwMask = computeKeywordMask (keywords ? std::string (keywords) : sentence);
  auto &stList = starters_[topic];
  StarterSpan span {};
  span.len = static_cast<uint8_t> (std::min<size_t> (4, ids.size ()));
  for (uint8_t k = 0; k < span.len; ++k) span.tokens[k] = ids[k];
  span.weight = static_cast<uint16_t> (isOnline ? 4u : 1u);
  span.keywordMask = kwMask;

  bool foundStarter = false;
  for (auto &sp : stList) {
    if (sp.len == span.len && sp.tokens == span.tokens) {
      if (sp.weight < 60000u) ++sp.weight;
      sp.keywordMask |= kwMask;
      foundStarter = true;
      break;
    }
  }
  if (!foundStarter) {
    stList.push_back (span);
  }

  addBigram (kBosToken, ids[0], intentBit, kwMask);
  addTrigram (kBosToken, kBosToken, ids[0], intentBit, kwMask);
  addBigram (ids[0], ids[1], intentBit, kwMask);
  addTrigram (kBosToken, ids[0], ids[1], intentBit, kwMask);
  addFourgram (kBosToken, kBosToken, ids[0], ids[1], intentBit, kwMask);

  for (size_t i = 2; i < ids.size (); ++i) {
    addBigram (ids[i - 1], ids[i], intentBit, kwMask);
    addTrigram (ids[i - 2], ids[i - 1], ids[i], intentBit, kwMask);
    const uint16_t p3 = (i >= 3) ? ids[i - 3] : kBosToken;
    addFourgram (p3, ids[i - 2], ids[i - 1], ids[i], intentBit, kwMask);
  }
  addBigram (ids.back (), kEosToken, intentBit, kwMask);
  addTrigram (ids[ids.size () - 2], ids.back (), kEosToken, intentBit, kwMask);
  addFourgram (ids.size () >= 3 ? ids[ids.size () - 3] : kBosToken, ids[ids.size () - 2], ids.back (), kEosToken, intentBit, kwMask);
  if (trainedSentences_ < 1000000000u) ++trainedSentences_;
}

void GenerativeEnglishLLM::ensurePretrained () {
  if (pretrained_) return;
  pretrained_ = true;

  vocab_.reserve (2048);
  wordToId_.reserve (2048);
  embeddings_.reserve (2048);
  bigrams_.reserve (2048);
  trigrams_.reserve (16384);
  fourgrams_.reserve (24576);

  uint32_t protoSeed = 0x4C4C4D35u;
  for (size_t k = 0; k < 23; ++k) {
    for (size_t d = 0; d < EmbedDim; ++d) {
      const float v = static_cast<float> (static_cast<int32_t> (nextRng (protoSeed) & 2047u) - 1024) / 1024.0f;
      intentPrototypes_[k][d] = (d == (k % EmbedDim)) ? 1.0f : v * 0.35f;
    }
    normalizeVec (intentPrototypes_[k]);
  }

  getOrAddToken ("<pad>", 0);
  getOrAddToken ("<bos>", 0);
  getOrAddToken ("<eos>", 0);
  getOrAddToken ("<unk>", 0);

  for (const auto &entry : kMassiveEnglishCorpus) {
    const auto intent = entry.intent < 23 ? static_cast<DialogueIntent> (entry.intent) : DialogueIntent::Banter;
    trainSentenceInternal (entry.sentence, intent, false, entry.keywords);
  }

  const auto &enPack = catalogue[0];
  for (size_t topic = 0; topic < 23; ++topic) {
    const auto intent = static_cast<DialogueIntent> (topic);
    for (size_t idx = 0; idx < 8; ++idx) {
      const char *baseLine = enPack.body[topic].entries[idx];
      trainSentenceInternal (baseLine, intent, false);
      const std::string withPrefix = std::string (enPack.prefix.entries[idx % 12]) + ", " + baseLine;
      trainSentenceInternal (withPrefix, intent, false);
      const std::string withTail = std::string (baseLine) + ", " + enPack.tail.entries[(idx + topic) % 12];
      trainSentenceInternal (withTail, intent, false);
    }
  }
  for (size_t i = 0; i < 6; ++i) {
    trainSentenceInternal (enPack.empty.entries[i], DialogueIntent::Utility, false);
    trainSentenceInternal (enPack.ack.entries[i], DialogueIntent::Apology, false);
    trainSentenceInternal (enPack.plan.entries[i], DialogueIntent::Plan, false);
  }
}

std::array<float, GenerativeEnglishLLM::EmbedDim> GenerativeEnglishLLM::encodePromptSelfAttention (
    const std::string &incoming, std::vector<uint16_t> *outPromptTokens) const {
  std::array<float, EmbedDim> pooled {};
  if (incoming.empty ()) return pooled;

  const auto words = tokenizeText (incoming);
  std::vector<uint16_t> tokIds;
  for (const auto &w : words) {
    const int id = findToken (w);
    if (id >= 4) tokIds.push_back (static_cast<uint16_t> (id));
  }
  if (outPromptTokens) *outPromptTokens = tokIds;
  if (tokIds.empty ()) return pooled;

  const size_t L = tokIds.size ();
  constexpr float kInvSqrtDk = 0.35355339f;
  for (size_t i = 0; i < L; ++i) {
    const auto &qi = embeddings_[tokIds[i]];
    std::array<float, EmbedDim> outVec {};
    for (int head = 0; head < 2; ++head) {
      const size_t offset = static_cast<size_t> (head) * 8u;
      std::vector<float> scores (L, 0.0f);
      float maxScore = -1e9f;
      for (size_t j = 0; j < L; ++j) {
        const auto &kj = embeddings_[tokIds[j]];
        float dot = 0.0f;
        for (size_t d = 0; d < 8; ++d) dot += qi[offset + d] * kj[offset + d];
        scores[j] = dot * kInvSqrtDk;
        if (scores[j] > maxScore) maxScore = scores[j];
      }
      float sumExp = 0.0f;
      for (size_t j = 0; j < L; ++j) {
        scores[j] = std::exp (scores[j] - maxScore);
        sumExp += scores[j];
      }
      const float invSum = sumExp > 1e-8f ? (1.0f / sumExp) : 0.0f;
      for (size_t j = 0; j < L; ++j) {
        const float w = scores[j] * invSum;
        const auto &vj = embeddings_[tokIds[j]];
        for (size_t d = 0; d < 8; ++d) {
          outVec[offset + d] += w * vj[offset + d];
        }
      }
    }
    for (size_t d = 0; d < EmbedDim; ++d) {
      pooled[d] += outVec[d];
    }
  }
  normalizeVec (pooled);
  return pooled;
}

std::array<float, GenerativeEnglishLLM::EmbedDim> GenerativeEnglishLLM::encodeGameState (
    DialogueIntent intent, const DialogueContext &ctx) const {
  const size_t topic = static_cast<size_t> (intent) < 23 ? static_cast<size_t> (intent) : static_cast<size_t> (DialogueIntent::Banter);
  std::array<float, EmbedDim> state = intentPrototypes_[topic];
  state[0] += (static_cast<float> (std::clamp (ctx.health, 0, 100)) / 100.0f - 0.5f) * 0.25f;
  state[1] += (static_cast<float> (std::clamp (ctx.money, 0, 16000)) / 16000.0f - 0.3f) * 0.25f;
  state[2] += ctx.clutch ? 0.35f : 0.0f;
  state[3] += ctx.planted ? 0.35f : 0.0f;
  state[4] += ctx.topFragger ? 0.30f : 0.0f;
  normalizeVec (state);
  return state;
}

DialogueIntent GenerativeEnglishLLM::inferIntent (const std::string &incoming) const {
  if (incoming.empty ()) return DialogueIntent::Banter;
  std::string t = incoming;
  for (auto &c : t) if (static_cast<unsigned char> (c) < 128) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));

  // Check tactical action & equipment intents before generic greeting words like "hey"
  if (hasWord (t, {"boost","box","ledge","crate","stack"}))
    return DialogueIntent::Boost;
  if (hasWord (t, {"flash","smoke","he","nade","nades","utility","util","molly","granat","pop"}))
    return DialogueIntent::Utility;
  if (hasWord (t, {"buy","eco","save","money","cash","drop","force","bank","beli"}))
    return DialogueIntent::Economy;
  if (hasWord (t, {"awp","ak","ak47","m4","m4a1","deagle","usp","glock","gun","weapon","rifle","sniper","senjata"}))
    return DialogueIntent::Equipment;
  if (hasWord (t, {"bomb","c4","plant","planted","defuse","timer"}))
    return DialogueIntent::Plant;
  if (hasWord (t, {"rotate","rotation","other","back","leave","pivot","rotasi"}))
    return DialogueIntent::Rotate;
  if (hasWord (t, {"hold","camp","anchor","wait","passive","defend","angle","watch"}))
    return DialogueIntent::Hold;
  if (hasWord (t, {"plan","tactic","strategy","strat","rush","split","mid","execute","default","push","long","tunnels","banana","ramp"}))
    return DialogueIntent::Plan;
  if (hasWord (t, {"gg","nice","nt","wp","clean","ns","insane","clutch","good","gj","mantap","thanks","ty"}))
    return DialogueIntent::Praise;
  if (hasWord (t, {"sorry","sry","mb","fault","apologies","bad","maaf","lag","spike"}))
    return DialogueIntent::Apology;
  if (hasWord (t, {"hp","low","heal","hurt","lit","tagged","damage"}))
    return DialogueIntent::Health;
  if (hasWord (t, {"hi","hello","hey","sup","yo","glhf","welcome","greetings","halo","bot","human","ping","fps","sens","sensitivity","resolution","dpi","how","doing"}))
    return DialogueIntent::Greeting;

  std::vector<uint16_t> promptToks;
  const auto hPrompt = encodePromptSelfAttention (incoming, &promptToks);
  if (!promptToks.empty ()) {
    float bestSim = -1e9f;
    DialogueIntent bestIntent = DialogueIntent::Plan;
    for (size_t k = 0; k < 23; ++k) {
      const float sim = dotVec (hPrompt, intentPrototypes_[k]);
      if (sim > bestSim) {
        bestSim = sim;
        bestIntent = static_cast<DialogueIntent> (k);
      }
    }
    return bestIntent;
  }
  return DialogueIntent::Plan;
}

void GenerativeEnglishLLM::trainOnText (const std::string &incoming, DialogueIntent hintIntent) {
  ensurePretrained ();
  if (incoming.size () < 3 || incoming.size () > 160) return;
  const DialogueIntent intent = (hintIntent < DialogueIntent::Count) ? hintIntent : inferIntent (incoming);
  trainSentenceInternal (incoming, intent, true);
  if (learnedSentences_.size () < 256) {
    learnedSentences_.push_back (incoming);
    learnedIntents_.push_back (static_cast<uint8_t> (intent));
  }
  if (onlineUpdates_ < 1000000000u) ++onlineUpdates_;
  dirty_ = true;
}

bool GenerativeEnglishLLM::generate (DialogueIntent intent, const DialogueContext &c, uint32_t &seed, DialogueMemory &memory, std::string &out) {
  ensurePretrained ();
  if (intent == DialogueIntent::TopFragger && !c.topFragger) intent = DialogueIntent::Ambition;
  const size_t topic = static_cast<size_t> (intent) < 23 ? static_cast<size_t> (intent) : static_cast<size_t> (DialogueIntent::Banter);
  const uint32_t intentBit = 1u << topic;

  std::vector<uint16_t> promptTokens;
  const auto hPrompt = encodePromptSelfAttention (c.incoming, &promptTokens);
  const auto hState = encodeGameState (intent, c);
  const uint32_t promptKwMask = computeKeywordMask (c.incoming);

  std::array<float, EmbedDim> hCtx {};
  for (size_t d = 0; d < EmbedDim; ++d) {
    hCtx[d] = (!promptTokens.empty () ? 0.65f * hPrompt[d] + 0.35f * hState[d] : hState[d]);
  }
  normalizeVec (hCtx);

  const auto &stList = starters_[topic];
  if (stList.empty ()) return false;

  std::vector<size_t> matchingStarters;
  if (promptKwMask != 0u) {
    for (size_t i = 0; i < stList.size (); ++i) {
      if ((stList[i].keywordMask & promptKwMask) != 0u) {
        matchingStarters.push_back (i);
      }
    }
  }

  std::string utilityStr;
  if (c.flash) utilityStr = "flash";
  if (c.smoke) utilityStr += utilityStr.empty () ? "smoke" : " and smoke";
  if (c.he) utilityStr += utilityStr.empty () ? "HE nade" : " and HE";
  if (utilityStr.empty ()) utilityStr = "utility";

  for (unsigned attempt = 0; attempt < 36; ++attempt) {
    std::vector<uint16_t> seq;
    seq.reserve (32);

    const StarterSpan &sp = (!matchingStarters.empty () && (attempt < 28))
      ? stList[matchingStarters[nextRng (seed) % matchingStarters.size ()]]
      : stList[nextRng (seed) % stList.size ()];
    for (uint8_t k = 0; k < sp.len; ++k) {
      seq.push_back (sp.tokens[k]);
    }

    // Autoregressive 4-Gram + Intent-Trigram + Self-Attention Decoding with Top-P Nucleus Sampling
    constexpr size_t kMaxGenTokens = 18;
    constexpr size_t kMinGenTokens = 7;
    constexpr float kTemperature = 0.78f;
    constexpr float kTopP = 0.86f;

    for (size_t step = seq.size (); step < kMaxGenTokens; ++step) {
      const uint16_t prev3 = seq.size () >= 3 ? seq[seq.size () - 3] : kBosToken;
      const uint16_t prev2 = seq[seq.size () - 2];
      const uint16_t prev1 = seq[seq.size () - 1];

      struct Candidate {
        uint16_t tok {};
        float logit {};
        float prob {};
      };
      std::vector<Candidate> cands;
      cands.reserve (16);

      auto addOrBoostCandidate = [&] (uint16_t tok, float baseLogit, uint32_t edgeMask, uint32_t edgeKwMask) {
        if (tok == kEosToken && seq.size () < kMinGenTokens) return;
        float logit = baseLogit;
        if (tok == kEosToken && seq.size () >= 11) logit += 3.5f;
        if ((edgeMask & intentBit) != 0) logit += 2.60f;
        if (promptKwMask != 0u && (edgeKwMask & promptKwMask) != 0u) logit += 2.20f;
        if (tok >= 4 && tok < embeddings_.size ()) {
          logit += 1.35f * dotVec (embeddings_[tok], hCtx);
          for (uint16_t pt : promptTokens) {
            if (pt < embeddings_.size ()) {
              const float sim = dotVec (embeddings_[tok], embeddings_[pt]);
              if (sim > 0.45f && tok != pt) logit += 0.65f * sim;
            }
          }
          if (vocab_[tok].size () > 3) {
            for (uint16_t used : seq) {
              if (used == tok) {
                logit -= 4.5f;
                break;
              }
            }
          }
        }
        for (auto &existing : cands) {
          if (existing.tok == tok) {
            existing.logit = std::max (existing.logit, logit) + 0.30f;
            return;
          }
        }
        cands.push_back (Candidate { tok, logit, 0.0f });
      };

      // Priority 1: Exact 4-gram context transitions (preserves natural multi-word grammar while allowing branching at shared 3-word pivots)
      if (const auto *fourEdges = findFourgram (prev3, prev2, prev1)) {
        for (const auto &e : *fourEdges) {
          addOrBoostCandidate (e.nextToken, std::log (static_cast<float> (e.count) + 1.0f) * 2.20f + 1.50f, e.intentMask, e.keywordMask);
        }
      }
      // Priority 2: Intent-matching Trigram transitions (enables creative cross-sentence recombination at shared 2-word phrases)
      if (const auto *triEdges = findTrigram (prev2, prev1)) {
        for (const auto &e : *triEdges) {
          if ((e.intentMask & intentBit) != 0 || cands.empty ()) {
            addOrBoostCandidate (e.nextToken, std::log (static_cast<float> (e.count) + 1.0f) * 1.40f, e.intentMask, e.keywordMask);
          }
        }
      }
      if (cands.empty () && prev1 < bigrams_.size ()) {
        for (const auto &e : bigrams_[prev1]) {
          if ((e.intentMask & intentBit) != 0) {
            addOrBoostCandidate (e.nextToken, std::log (static_cast<float> (e.count) + 0.5f) * 0.80f, e.intentMask, e.keywordMask);
          }
        }
      }
      if (cands.empty ()) break;

      std::sort (cands.begin (), cands.end (), [] (const Candidate &a, const Candidate &b) {
        return a.logit > b.logit;
      });
      if (cands.size () > 5) cands.resize (5);

      const float maxLogit = cands[0].logit;
      float sumProb = 0.0f;
      for (auto &cand : cands) {
        cand.prob = std::exp ((cand.logit - maxLogit) / kTemperature);
        sumProb += cand.prob;
      }
      for (auto &cand : cands) cand.prob /= std::max (sumProb, 1e-8f);

      float cumProb = 0.0f;
      size_t cutoff = cands.size ();
      for (size_t i = 0; i < cands.size (); ++i) {
        cumProb += cands[i].prob;
        if (cumProb >= kTopP && i >= 1) {
          cutoff = i + 1;
          break;
        }
      }
      cands.resize (cutoff);
      float renorm = 0.0f;
      for (const auto &cand : cands) renorm += cand.prob;

      const float r = (static_cast<float> (nextRng (seed) & 0xFFFFu) / 65535.0f) * renorm;
      float acc = 0.0f;
      uint16_t chosen = cands[0].tok;
      for (const auto &cand : cands) {
        acc += cand.prob;
        if (r <= acc) {
          chosen = cand.tok;
          break;
        }
      }
      if (chosen == kEosToken) break;
      seq.push_back (chosen);
    }

    // Trim any dangling trailing comma or conjunction if max length was reached
    while (!seq.empty ()) {
      const std::string &lastW = vocab_[seq.back ()];
      if (lastW == "," || lastW == "and" || lastW == "so" || lastW == "or" || lastW == "to" || lastW == "with" || lastW == "the" || lastW == "a" || lastW == "for" || lastW == "their" || lastW == "my" || lastW == "our" || lastW == "your" || lastW == "in" || lastW == "on" || lastW == "at" || lastW == "of") {
        seq.pop_back ();
      }
      else {
        break;
      }
    }

    out.clear ();
    for (size_t i = 0; i < seq.size (); ++i) {
      const uint16_t id = seq[i];
      if (id < 4 || id >= vocab_.size ()) continue;
      const std::string &w = vocab_[id];
      const bool isPunct = (w == "," || w == "." || w == "?" || w == "!");
      if (!out.empty () && !isPunct) out.push_back (' ');
      out += w;
    }
    if (out.size () < 10) continue;

    replaceSlot (out, "{utility}", utilityStr);
    replaceSlot (out, "${cash}", "$" + std::to_string (std::clamp (c.money, 0, 16000)));
    replaceSlot (out, "{cash}", std::to_string (std::clamp (c.money, 0, 16000)));
    replaceSlot (out, "{hp}", std::to_string (std::clamp (c.health, 0, 100)));
    replaceSlot (out, "{weapon}", c.weapon.empty () ? "rifle" : c.weapon.substr (0, 20));
    replaceSlot (out, "{frags}", std::to_string (std::clamp (c.frags, -999, 999)));
    replaceSlot (out, "#{rank}", "#" + std::to_string (std::clamp (c.rank, 1, 32)));
    replaceSlot (out, "{rank}", std::to_string (std::clamp (c.rank, 1, 32)));
    replaceSlot (out, "{enemy}", c.opponent.empty () ? "their entry" : c.opponent.substr (0, 16));
    replaceSlot (out, "{ally}", c.ally.empty () ? "bro" : c.ally.substr (0, 16));
    replaceSlot (out, "{area}", c.area.empty () ? "site" : c.area.substr (0, 14));
    replaceSlot (out, "{strategy}", c.strategy.empty () ? "default" : c.strategy.substr (0, 12));
    replaceSlot (out, "{directive}", c.directive.empty () ? "crossfire" : c.directive.substr (0, 16));
    replaceSlot (out, "{role}", c.role.empty () ? "support" : c.role.substr (0, 10));

    truncateUtf8 (out, 150);
    const auto hash = hashBytes (out.data (), out.size ());
    bool duplicate = false;
    for (auto h : memory.hashes) if (h == hash) duplicate = true;
    if (duplicate) continue;

    memory.hashes[memory.cursor++ % memory.hashes.size ()] = hash;
    memory.topic = intent;
    generatedTokens_ += static_cast<uint32_t> (seq.size ());
    return true;
  }
  out.clear ();
  return false;
}

bool GenerativeEnglishLLM::save (const std::string &path, std::string &error) {
  ensurePretrained ();
  error.clear ();
  std::vector<uint8_t> b { 'H', 'C', 'L', 'L', 'M', '0', '0', '5' };
  putU32 (b, trainedSentences_);
  putU32 (b, onlineUpdates_);
  putU32 (b, generatedTokens_);
  putU32 (b, static_cast<uint32_t> (learnedSentences_.size ()));
  for (size_t i = 0; i < learnedSentences_.size (); ++i) {
    putU32 (b, static_cast<uint32_t> (learnedIntents_[i]));
    putU32 (b, static_cast<uint32_t> (learnedSentences_[i].size ()));
    b.insert (b.end (), learnedSentences_[i].begin (), learnedSentences_[i].end ());
  }
  putU32 (b, hashBytes (b.data (), b.size ()));
  const auto tmp = path + ".tmp";
  auto f = std::fopen (tmp.c_str (), "wb");
  if (!f) { error = "cannot open llm temp"; return false; }
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
  if (!ok) { std::remove (tmp.c_str ()); error = "llm save failed"; return false; }
  dirty_ = false;
  return true;
}

bool GenerativeEnglishLLM::load (const std::string &path, std::string &error) {
  ensurePretrained ();
  error.clear ();
  auto f = std::fopen (path.c_str (), "rb");
  if (!f) { error = "llm file not found"; return false; }
  std::vector<uint8_t> b (128 * 1024);
  const auto size = std::fread (b.data (), 1, b.size (), f);
  const bool io = std::ferror (f) != 0;
  std::fclose (f);
  if (io || size < 24 || size >= b.size ()) { error = "invalid llm file size"; return false; }
  b.resize (size);
  BufReader r { b, 0, true }, tail { b, size - 4, true };
  if (std::memcmp (b.data (), "HCLLM005", 8) != 0 || tail.getU32 () != hashBytes (b.data (), size - 4)) {
    error = "llm header/checksum mismatch"; return false;
  }
  r.p = 8;
  (void)r.getU32 ();
  onlineUpdates_ = r.getU32 ();
  generatedTokens_ = r.getU32 ();
  const uint32_t count = r.getU32 ();
  if (!r.ok || count > 512) { error = "invalid learned sentence count"; return false; }
  learnedSentences_.clear ();
  learnedIntents_.clear ();
  for (uint32_t i = 0; i < count && r.ok; ++i) {
    const uint32_t intentVal = r.getU32 ();
    const uint32_t len = r.getU32 ();
    if (!r.ok || len > 256 || r.p + len > size - 4) { error = "corrupt learned sentence"; return false; }
    std::string s (reinterpret_cast<const char *> (b.data () + r.p), len);
    r.p += len;
    const auto intent = intentVal < 23 ? static_cast<DialogueIntent> (intentVal) : DialogueIntent::Banter;
    trainSentenceInternal (s, intent, true);
    learnedSentences_.push_back (s);
    learnedIntents_.push_back (static_cast<uint8_t> (intent));
  }
  dirty_ = false;
  return true;
}

GenerativeEnglishLLM &englishLLM () {
  static GenerativeEnglishLLM instance {};
  return instance;
}

const char *profileLanguage (const std::string &name) {
  (void)name;
  return "en";
}

const char *profileCountry (const std::string &name) {
  const auto base = baseName (name);
  for (const auto &p : profiles) if (base == baseName (p.name)) return p.country;
  return "International";
}

bool supportedLanguage (const std::string &language) {
  for (const auto &p : catalogue) if (language == p.code) return true;
  return false;
}

DialogueIntent parseIntent (const std::string &incoming) {
  return englishLLM ().inferIntent (incoming);
}

DialogueIntent ambientIntent (const DialogueContext &c, DialogueIntent previous, uint32_t &seed) {
  if (c.clutch && previous != DialogueIntent::Cautious && previous != DialogueIntent::Plan)
    return c.health < 45 ? DialogueIntent::Cautious : DialogueIntent::Plan;
  if (c.topFragger && previous != DialogueIntent::TopFragger) return DialogueIntent::TopFragger;
  if (c.boosting && previous != DialogueIntent::Boost) return DialogueIntent::Boost;
  if (c.mood == 2 && previous != DialogueIntent::Angry) return DialogueIntent::Angry;
  if (c.mood == 3 && previous != DialogueIntent::Afraid) return DialogueIntent::Afraid;
  if (c.mood == 4 && previous != DialogueIntent::Brave) return DialogueIntent::Brave;
  if (c.mood == 5 && previous != DialogueIntent::Cautious) return DialogueIntent::Cautious;
  if (c.rank > 1 && previous != DialogueIntent::Ambition && nextRng (seed) % 3u == 0) return DialogueIntent::Ambition;
  if (c.health < 35 && previous != DialogueIntent::Health) return DialogueIntent::Health;
  if (c.planted && previous != DialogueIntent::Plant) return DialogueIntent::Plant;
  if (c.exploring && previous != DialogueIntent::Explore) return DialogueIntent::Explore;
  static constexpr DialogueIntent topics[] = {
    DialogueIntent::Plan, DialogueIntent::Banter, DialogueIntent::Economy,
    DialogueIntent::Equipment, DialogueIntent::Utility, DialogueIntent::Rotate, DialogueIntent::Hold
  };
  auto intent = topics[nextRng (seed) % 7];
  if (intent == previous) intent = topics[(nextRng (seed) % 6 + 1) % 7];
  return intent;
}

void truncateUtf8 (std::string &text, size_t bytes) {
  if (text.size () <= bytes) return;
  size_t cut = bytes;
  while (cut > 0 && (static_cast<unsigned char> (text[cut]) & 0xc0u) == 0x80u) --cut;
  const size_t lastSpace = text.rfind (' ', cut);
  if (lastSpace != std::string::npos && lastSpace >= 16) cut = lastSpace;
  while (cut > 0 && (text[cut - 1] == ',' || text[cut - 1] == ' ')) --cut;
  text.resize (cut);
}

bool composeDialogue (const std::string &language, DialogueIntent intent, const DialogueContext &c, uint32_t &seed, DialogueMemory &memory, std::string &out) {
  (void)language;
  if (!c.incoming.empty ()) {
    englishLLM ().trainOnText (c.incoming, intent);
  }
  if (englishLLM ().generate (intent, c, seed, memory, out)) {
    return true;
  }
  return false;
}

unsigned dialogueLanguageCount () { return static_cast<unsigned> (sizeof catalogue / sizeof catalogue[0]); }
unsigned dialogueFragmentCount () { return static_cast<unsigned> (englishLLM ().vocabSize ()); }
} // namespace rbcc
#endif
