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
