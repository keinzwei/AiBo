// Generative English LLM Dialogue Engine (Massive 3,600+ Sentence Corpus, 4-Gram + Self-Attention & Top-P Decoding). GPL-2.0-or-later.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rbcc {
enum class DialogueIntent : uint8_t { Greeting, Praise, Apology, Utility, Economy, Equipment, Kill, Dead, Plant, Explore, Rotate, Hold, Plan, Unknown, Banter, Health,
  TopFragger, Angry, Afraid, Brave, Cautious, Ambition, Boost, Count };
struct DialogueContext {
  int money {}, health {100};
  bool flash {}, smoke {}, he {}, exploring {}, planted {}, reply {};
  std::string weapon {"rifle"};
  bool topFragger {}, boosting {};
  int mood {}, rank {1}, frags {};
  std::string opponent {};
  std::string ally {};
  std::string area {};
  std::string strategy {};
  std::string directive {};
  std::string role {};
  std::string subgoal {};
  std::string incoming {};
  int aliveAllies {5}, aliveEnemies {5}, streak {};
  bool clutch {}, revenge {}, headshot {};
};
struct DialogueMemory {
  std::array<uint32_t,32> hashes {};
  unsigned cursor {};
  DialogueIntent topic {DialogueIntent::Banter};
};

// Skill 3: In-DLL Generative English Language Model trained on Massive 3,600+ Sentence English Database
class GenerativeEnglishLLM final {
public:
  static constexpr size_t EmbedDim = 16;
  static constexpr size_t MaxVocab = 4096;
  static constexpr uint16_t kPadToken = 0;
  static constexpr uint16_t kBosToken = 1;
  static constexpr uint16_t kEosToken = 2;
  static constexpr uint16_t kUnkToken = 3;

  struct TransitionEdge {
    uint16_t nextToken {};
    uint16_t count { 1 };
    uint32_t intentMask {};
    uint32_t keywordMask {};
  };
  struct StarterSpan {
    std::array<uint16_t, 4> tokens {};
    uint8_t len {};
    uint16_t weight { 1 };
    uint32_t keywordMask {};
  };

private:
  std::vector<std::string> vocab_ {};
  std::unordered_map<std::string, uint16_t> wordToId_ {};
  std::vector<std::array<float, EmbedDim>> embeddings_ {};
  std::array<std::array<float, EmbedDim>, 23> intentPrototypes_ {};
  std::vector<std::vector<TransitionEdge>> bigrams_ {};
  std::unordered_map<uint32_t, std::vector<TransitionEdge>> trigrams_ {};
  std::unordered_map<uint64_t, std::vector<TransitionEdge>> fourgrams_ {};
  std::array<std::vector<StarterSpan>, 23> starters_ {};
  std::vector<std::string> learnedSentences_ {};
  std::vector<uint8_t> learnedIntents_ {};
  uint32_t trainedSentences_ {};
  uint32_t corpusTokens_ {};
  uint32_t onlineUpdates_ {};
  uint32_t generatedTokens_ {};
  bool pretrained_ { false };
  bool dirty_ { false };

  void ensurePretrained ();
  uint16_t getOrAddToken (const std::string &word, uint32_t intentBit = 0);
  int findToken (const std::string &word) const;
  std::vector<std::string> tokenizeText (const std::string &text) const;
  uint32_t computeKeywordMask (const std::string &text) const;
  void addBigram (uint16_t prev1, uint16_t nextTok, uint32_t intentBit, uint32_t kwMask);
  void addTrigram (uint16_t prev2, uint16_t prev1, uint16_t nextTok, uint32_t intentBit, uint32_t kwMask);
  void addFourgram (uint16_t prev3, uint16_t prev2, uint16_t prev1, uint16_t nextTok, uint32_t intentBit, uint32_t kwMask);
  const std::vector<TransitionEdge> *findTrigram (uint16_t prev2, uint16_t prev1) const;
  const std::vector<TransitionEdge> *findFourgram (uint16_t prev3, uint16_t prev2, uint16_t prev1) const;
  void trainSentenceInternal (const std::string &sentence, DialogueIntent intent, bool isOnline, const char *keywords = nullptr);

public:
  GenerativeEnglishLLM () { ensurePretrained (); }
  void clear ();
  std::array<float, EmbedDim> encodePromptSelfAttention (const std::string &incoming, std::vector<uint16_t> *outPromptTokens = nullptr) const;
  std::array<float, EmbedDim> encodeGameState (DialogueIntent intent, const DialogueContext &ctx) const;
  DialogueIntent inferIntent (const std::string &incoming) const;
  void trainOnText (const std::string &incoming, DialogueIntent hintIntent = DialogueIntent::Count);
  bool generate (DialogueIntent intent, const DialogueContext &ctx, uint32_t &seed, DialogueMemory &memory, std::string &out);
  bool save (const std::string &path, std::string &error);
  bool load (const std::string &path, std::string &error);
  size_t vocabSize () const { return vocab_.size (); }
  size_t trigramStates () const { return trigrams_.size () + fourgrams_.size (); }
  uint32_t trainedSentences () const { return trainedSentences_; }
  uint32_t corpusTokens () const { return corpusTokens_; }
  uint32_t onlineUpdates () const { return onlineUpdates_; }
  uint32_t generatedTokens () const { return generatedTokens_; }
  bool dirty () const { return dirty_; }
};

GenerativeEnglishLLM &englishLLM ();

const char *profileLanguage (const std::string &name);
const char *profileCountry (const std::string &name);
bool supportedLanguage (const std::string &language);
DialogueIntent parseIntent (const std::string &incoming);
DialogueIntent ambientIntent (const DialogueContext &context, DialogueIntent previous, uint32_t &seed);
bool composeDialogue (const std::string &language, DialogueIntent intent, const DialogueContext &context,
  uint32_t &seed, DialogueMemory &memory, std::string &out);
void truncateUtf8 (std::string &text, size_t bytes);
unsigned dialogueLanguageCount ();
unsigned dialogueFragmentCount ();
}
