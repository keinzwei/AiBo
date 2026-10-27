// Local multilingual procedural dialogue. GPL-2.0-or-later.
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace rbcc {
enum class DialogueIntent : uint8_t { Greeting, Praise, Apology, Utility, Economy, Equipment, Kill, Dead, Plant, Explore, Rotate, Hold, Plan, Unknown, Banter, Health,
  TopFragger, Angry, Afraid, Brave, Cautious, Ambition, Boost, Count };
struct DialogueContext {
  int money {}, health {100};
  bool flash {}, smoke {}, he {}, exploring {}, planted {}, reply {};
  std::string weapon {"rifle"};
  bool topFragger {}, boosting {};
  int mood {}, rank {1}, frags {};
};
struct DialogueMemory {
  std::array<uint32_t,32> hashes {};
  unsigned cursor {};
  DialogueIntent topic {DialogueIntent::Banter};
};
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
