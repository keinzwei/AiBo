// YaPB Human Copycat 4.2.7. SPDX-License-Identifier: GPL-2.0-or-later
#include <aibo.h>
#include <initializer_list>
#include "copycat/copycat_dialogue.h"
#include <algorithm>
#include <string>

namespace bot {
namespace {
ystl::FixedArray<uint32_t, 64> chat_history {};
size_t chat_history_cursor {};
float chat_global_next {};
float chat_ambient_next {};
int chat_ambient_last {-1};
struct DialogueActor {
  rbcc::DialogueMemory memory {};
  uint32_t seed {1}, name {};
  int serial {-1}, user {-1};
  rbcc::DialogueIntent generated {rbcc::DialogueIntent::Banter};
  bool reply {}, may_reply {};
  bool scope_alive {};
  Team scope_team {Team::Invalid};
  int reply_sender {-1}, reply_serial {-1};
  uint32_t reply_name {};
  Team reply_team {Team::Invalid};
};
DialogueActor dialogue[kGameMaxPlayers];
struct Conversation {
  int sender {-1}, serial {-1};
  uint32_t name {};
  Team team {Team::Invalid}; bool team_only {}, alive {};
  float ready {}, expires {};
  rbcc::DialogueIntent topic {rbcc::DialogueIntent::Greeting};
} conversation;
unsigned generated_lines {}, conversation_replies {};
DialogueActor &DialogueState (Bot &bot) {
  auto &s = dialogue[bot.Index ()]; const auto name = bot.pev->netname.str ().hash ();
  const int user = engfuncs.pfnGetPlayerUserId (bot.Ent ());
  if (s.serial != bot.Ent ()->serialnumber || s.user != user || s.name != name) {
    s = DialogueActor {}; s.serial = bot.Ent ()->serialnumber; s.user = user; s.name = name;
    s.seed = name ^ static_cast<uint32_t> (bot.Index () + 1) * 2654435761u ^ static_cast<uint32_t> (s.serial);
  }
  return s;
}
rbcc::DialogueContext DialogueContext (const Bot &bot) {
  rbcc::DialogueContext c; c.money = bot.money_amount_; c.health = static_cast<int> (std::clamp (bot.pev->health,0.0f,100.0f));
  const auto w = static_cast<uint32_t> (bot.pev->weapons);
  c.flash = (w & (1u << 25u)) != 0; c.smoke = (w & (1u << 9u)) != 0; c.he = (w & (1u << 4u)) != 0;
  c.exploring = HumanExplore::Active (bot); c.planted = game_state.IsBombPlanted ();
  c.topFragger = HumanArena::Top (bot); c.boosting = HumanArena::Boosting (bot);
  c.mood = HumanArena::Mood (bot); c.rank = HumanArena::Rank (bot); c.frags = static_cast<int> (bot.pev->frags);
  switch (bot.current_weapon_) {
  case Weapon::AK47: c.weapon = "AK-47"; break;
  case Weapon::M4A1: c.weapon = "M4A1"; break;
  case Weapon::AWP: c.weapon = "AWP"; break;
  case Weapon::Deagle: c.weapon = "Desert Eagle"; break;
  case Weapon::USP: c.weapon = "USP"; break;
  case Weapon::Glock18: c.weapon = "Glock-18"; break;
  case Weapon::Knife: c.weapon = "knife"; break;
  case Weapon::C4: c.weapon = "C4"; break;
  default: c.weapon = "utility"; break;
  }
  return c;
}
bool ModernDialogue (Bot &bot, Chat event, ystl::String &out, ystl::StringRef incoming,
    bool reply = false, rbcc::DialogueIntent replying = rbcc::DialogueIntent::Unknown) {
  auto &s = DialogueState (bot); auto c = DialogueContext (bot); c.reply = reply || !incoming.empty ();
  auto intent = rbcc::DialogueIntent::Banter;
  if (reply) intent = replying;
  else if (!incoming.empty ()) intent = rbcc::parseIntent (std::string (incoming.chars (), incoming.size ()));
  else switch (event) {
    case Chat::Hello: intent = rbcc::DialogueIntent::Greeting; break;
    case Chat::Kill: intent = rbcc::DialogueIntent::Kill; break;
    case Chat::Dead: intent = rbcc::DialogueIntent::Dead; break;
    case Chat::Plant: intent = rbcc::DialogueIntent::Plant; break;
    case Chat::TeamAttack: case Chat::TeamKill: intent = rbcc::DialogueIntent::Apology; break;
    default: intent = rbcc::ambientIntent (c,s.memory.topic,s.seed); break;
  }
  for (unsigned attempt = 0; attempt < 12; ++attempt) {
    std::string message;
    if (!rbcc::composeDialogue (HumanChat::Language (bot),intent,c,s.seed,s.memory,message)) return false;
    out.assign (message.c_str ()); const auto hash = out.hash (); bool repeated = false;
    for (auto h : chat_history) if (h == hash) repeated = true;
    if (repeated) continue;
    chat_history[chat_history_cursor++ % chat_history.size ()] = hash;
    s.generated = intent; s.reply = reply; s.may_reply = incoming.empty () && !reply;
    s.scope_alive = bot.is_alive_; s.scope_team = bot.team_;
    s.reply_sender = reply ? conversation.sender : -1; s.reply_serial = reply ? conversation.serial : -1;
    s.reply_name = conversation.name; s.reply_team = conversation.team;
    ++generated_lines; return true;
  }
  out.clear (); return false;
}

const char *ChatFragment (ystl::RWrand &random, std::initializer_list<const char *> choices) {
  return *(choices.begin () + random (0, static_cast<int> (choices.size ()) - 1));
}
bool ChatWord (ystl::StringRef text, ystl::StringRef word) {
  size_t start = 0;
  while (start < text.size ()) {
    const auto pos = text.find (word, start);
    if (pos == ystl::String::InvalidIndex) return false;
    const auto end = pos + word.size ();
    if ((pos == 0 || !isalnum (static_cast<unsigned char> (text[pos - 1]))) &&
        (end == text.size () || !isalnum (static_cast<unsigned char> (text[end])))) return true;
    start = end;
  }
  return false;
}
} // namespace

void HumanChat::Reset () {
  for (auto &hash : chat_history) hash = 0;
  chat_history_cursor = 0; chat_global_next = 0.0f;
  chat_ambient_next = game.Time () + 4.0f; chat_ambient_last = -1;
  for (auto &s : dialogue) s = DialogueActor {};
  conversation = Conversation {}; generated_lines = conversation_replies = 0;
}

const char *HumanChat::Language (const Bot &bot) {
  const auto configured = cv_human_chat_language.As<ystl::StringRef> ();
  if (rbcc::supportedLanguage (std::string (configured.chars (), configured.size ()))) return cv_human_chat_language.ptr->string;
  return rbcc::profileLanguage (bot.pev->netname.chars ());
}
void HumanChat::NewRound () { conversation = Conversation {}; }

bool HumanChat::Generate (Bot &bot, Chat event, ystl::String &out, ystl::StringRef incoming) {
  out.clear ();
  if (!cv_chat || !cv_human_generative_chat) return false;
  if (cv_human_chat_catalogue || cv_human_chat_language.As<ystl::StringRef> () == "profile" ||
      cv_human_chat_language.As<ystl::StringRef> () == "mix" || cv_human_chat_language.As<ystl::StringRef> () == "auto")
    return ModernDialogue (bot,event,out,incoming);
  ystl::String text (incoming);
  text.lowercase ();
  enum class Intent { Greeting, Praise, Apology, Utility, Economy, Equipment, Unknown, Banter, Kill, Dead, Plant };
  Intent intent = Intent::Banter;
  if (!text.empty ()) {
    intent = Intent::Unknown;
    if (ChatWord (text, "hi") || ChatWord (text, "hello") || ChatWord (text, "halo") || ChatWord (text, "glhf")) intent = Intent::Greeting;
    else if (ChatWord (text, "gg") || ChatWord (text, "nice") || ChatWord (text, "nt") || ChatWord (text, "mantap")) intent = Intent::Praise;
    else if (ChatWord (text, "sorry") || ChatWord (text, "maaf") || ChatWord (text, "sry")) intent = Intent::Apology;
    else if (ChatWord (text, "flash") || ChatWord (text, "smoke") || ChatWord (text, "cover") || ChatWord (text, "bantu")) intent = Intent::Utility;
    else if (ChatWord (text, "eco") || ChatWord (text, "save") || ChatWord (text, "buy") || ChatWord (text, "beli")) intent = Intent::Economy;
    else if (ChatWord (text, "awp") || ChatWord (text, "deagle") || ChatWord (text, "weapon") || ChatWord (text, "senjata")) intent = Intent::Equipment;
    else if (ChatWord (text, "bot") || ChatWord (text, "cheat") || ChatWord (text, "aimbot")) intent = Intent::Banter;
  }
  else {
    switch (event) {
    case Chat::Hello: intent = Intent::Greeting; break;
    case Chat::Kill: intent = Intent::Kill; break;
    case Chat::Dead: intent = Intent::Dead; break;
    case Chat::Plant: intent = Intent::Plant; break;
    case Chat::TeamAttack:
    case Chat::TeamKill: intent = Intent::Apology; break;
    default: break;
    }
  }
  auto &random = bot.rg;
  const bool id = cv_human_chat_language.As<ystl::StringRef> () != "en";
  const auto style = bot.pev->netname.str ().hash () % 3u;
  for (unsigned attempt = 0; attempt < 10; ++attempt) {
    out.clear ();
    auto pick = [&] (std::initializer_list<const char *> options) { out.append (ChatFragment (random, options)); };
    if (style != 0 && random.chance (35)) pick (id ? std::initializer_list<const char *> {"oke, ", "hmm, ", "yah, ", "sip, "}
      : std::initializer_list<const char *> {"ok, ", "well, ", "yeah, ", "hmm, "});
    if (id) {
      switch (intent) {
      case Intent::Greeting:
        pick ({"halo", "hai", "halo tim", "yo"}); pick ({" semua", " guys", " teman-teman", " tim"});
        pick ({", semoga", ", mudah-mudahan", ", moga"}); pick ({" mainnya seru", " lancar round ini", " kompak mainnya", " dapet match seru"}); break;
      case Intent::Praise:
        pick ({"tadi", "round itu", "barusan"}); pick ({" rapi", " bagus banget", " keren", " mantap"});
        pick ({", timingnya", ", peekingnya", ", tembakannya"}); pick ({" pas", " enak", " bersih", " oke"}); break;
      case Intent::Apology:
        if (!text.empty ()) { pick ({"santai", "gapapa", "aman", "sudah"}); pick ({", lanjut", ", fokus lagi", ", kita lanjut", ", main lagi"}); }
        else { pick ({"maaf", "sorry", "aduh maaf"}); pick ({", tadi", ", barusan"}); pick ({" salah timing", " kepencet", " kurang hati-hati", " salah gerak"}); }
        break;
      case Intent::Utility:
        if (has_flag (bot.pev->weapons, ystl::bit (Weapon::Flashbang))) { pick ({"aku", "gw", "saya"}); pick ({" masih pegang", " masih punya", " ada"}); out.append (" flash"); }
        else if (has_flag (bot.pev->weapons, ystl::bit (Weapon::Smoke))) { pick ({"aku", "gw", "saya"}); pick ({" masih bawa", " ada", " masih punya"}); out.append (" smoke"); }
        else { pick ({"utility", "granatku", "flashku"}); pick ({" sudah habis", " lagi kosong", " udah kepakai"}); }
        pick ({", cari timing dulu", ", lihat situasi dulu", ", pelan dulu", ", jangan maksa"}); break;
      case Intent::Economy:
        pick ({"uangku", "duitku", "budgetku"});
        if (bot.money_amount_ < 2700) pick ({" tipis", " belum cukup buat rifle", " kurang buat full buy"});
        else pick ({" cukup buat beli", " lumayan aman", " masih ada"});
        pick ({", sesuaikan aja", ", lihat ekonomi tim", ", main kompak aja", ", jangan buang-buang"}); break;
      case Intent::Equipment:
        pick ({"aku", "gw", "saya"}); pick ({" sekarang pegang ", " lagi pakai ", " sekarang bawa "});
        out.append (conf.FindWeaponById (bot.current_weapon_).alias); pick ({", coba cari timing", ", semoga cocok", ", lihat nanti", ", main sabar dulu"}); break;
      case Intent::Kill:
        pick ({"tadi", "barusan", "yang itu"}); pick ({" hampir lolos", " lumayan mepet", " akhirnya kena", " bikin kaget"});
        pick ({", mesti", ", perlu", ", harus"}); pick ({" fokus lagi", " jaga timing", " lebih sabar", " rapihin aim"}); break;
      case Intent::Dead:
        pick ({"aku tadi", "gw tadi", "barusan"}); pick ({" terlalu maju", " telat respon", " salah timing", " kurang sabar", " keburu peek"});
        pick ({", next round", ", habis ini", ", round berikutnya"}); pick ({" coba lebih sabar", " ganti tempo", " rapihin lagi", " coba cara lain"}); break;
      case Intent::Plant:
        pick ({"bomb", "bom"}); pick ({" sudah terpasang", " sudah aktif", " udah dipasang"});
        pick ({", sekarang", ", tinggal", ", waktunya"}); pick ({" jaga tempo", " main sabar", " tahan posisi", " fokus bareng"}); break;
      case Intent::Unknown:
        pick ({"aku", "gw", "saya"}); pick ({" belum nangkep", " kurang paham", " tadi kurang baca"});
        pick ({", maksudnya gimana?", ", coba ulang singkat?", ", ngomong yang mana?", ", lagi bahas apa?"}); break;
      case Intent::Banter:
        pick ({"aku", "gw", "saya"}); pick ({" lagi coba", " masih belajar", " pengen lebih rapi soal"});
        pick ({" timing peek", " recoil", " gerakan", " posisi"}); pick ({", pelan-pelan", ", belum konsisten", ", kadang meleset", ", coba lagi nanti"}); break;
      }
    }
    else {
      switch (intent) {
      case Intent::Greeting: pick ({"hey", "hi", "hello", "yo"}); pick ({" everyone", " team", " folks"}); pick ({", have a", ", hoping for a"}); pick ({" fun game", " good match", " clean round"}); break;
      case Intent::Praise: pick ({"that was", "the last round was", "your timing was"}); pick ({" really clean", " pretty solid", " nice"}); pick ({", keep it up", ", good stuff", ", well played"}); break;
      case Intent::Apology: pick (text.empty () ? std::initializer_list<const char *> {"sorry", "my bad", "oops"} : std::initializer_list<const char *> {"all good", "no worries", "it's fine"}); pick ({", let's", ", we can"}); pick ({" reset", " focus again", " keep going"}); break;
      case Intent::Utility: pick ({"my utility is", "my nades are"}); pick (has_flag (bot.pev->weapons, ystl::bit (Weapon::Flashbang) | ystl::bit (Weapon::Smoke) | ystl::bit (Weapon::Explosive)) ? std::initializer_list<const char *> {" still available", " ready"} : std::initializer_list<const char *> {" already used", " gone"}); pick ({", check the timing", ", play it slow", ", don't rush it"}); break;
      case Intent::Economy: pick ({"my money is", "my budget is"}); pick (bot.money_amount_ < 2700 ? std::initializer_list<const char *> {" pretty low", " tight"} : std::initializer_list<const char *> {" looking ok", " enough for a buy"}); pick ({", check the team economy", ", let's be careful", ", play together"}); break;
      case Intent::Equipment: pick ({"I'm holding ", "I've got ", "I'm using "}); out.append (conf.FindWeaponById (bot.current_weapon_).alias); pick ({", looking for the timing", ", taking it slow", ", need a good angle"}); break;
      case Intent::Kill: pick ({"that one", "the last peek", "that fight"}); pick ({" was close", " nearly got away", " caught me off guard"}); pick ({", stay focused", ", need to reset", ", timing matters"}); break;
      case Intent::Dead: pick ({"I", "guess I"}); pick ({" pushed too far", " reacted late", " peeked too early", " mistimed that"}); pick ({", next round", ", after this"}); pick ({" I'll slow down", " I'll change the pace", " I'll try another angle"}); break;
      case Intent::Plant: pick ({"bomb is", "the bomb is"}); pick ({" planted", " ticking", " set"}); pick ({", hold together", ", play the clock", ", stay patient"}); break;
      case Intent::Unknown: pick ({"I", "sorry I"}); pick ({" missed that", " didn't catch that", " lost the thread"}); pick ({", what do you mean?", ", say that again?", ", which part?"}); break;
      case Intent::Banter: pick ({"I'm", "still"}); pick ({" working on", " trying to improve"}); pick ({" my timing", " my recoil", " my positioning"}); pick ({", a bit inconsistent", ", one round at a time", ", getting there"}); break;
      }
    }
    if (style == 2 && random.chance (20)) pick (id ? std::initializer_list<const char *> {" hehe", " wkwk", " :)"} : std::initializer_list<const char *> {" haha", " :)", " lol"});
    if (out.size () > 150) out.erase (150, out.size () - 150);
    const auto hash = out.hash ();
    bool repeated = false;
    for (const auto recent : bot.generated_chat_hashes_) if (recent == hash) repeated = true;
    for (const auto recent : chat_history) if (recent == hash) repeated = true;
    if (repeated) continue;
    bot.generated_chat_hashes_[bot.generated_chat_cursor_++ % bot.generated_chat_hashes_.size ()] = hash;
    chat_history[chat_history_cursor++ % chat_history.size ()] = hash;
    return true;
  }
  out.clear (); return false;
}

bool HumanChat::Queue (Bot &bot, Chat event, bool team, ystl::StringRef incoming) {
  if (!cv_chat || !cv_human_generative_chat || bot.is_creature_ || !bot.pending_generated_chat_.empty () ||
      game.Time () < bot.next_generated_chat_) return false;
  if (!Generate (bot, event, bot.pending_generated_chat_, incoming)) return false;
  bot.pending_generated_team_ = team;
  const float delay = 0.8f + static_cast<float> (bot.pending_generated_chat_.size ()) * bot.rg (0.025f, 0.055f);
  bot.pending_generated_time_ = game.Time () + delay;
  bot.next_generated_chat_ = game.Time () + delay + cv_human_chat_interval.As<float> () + bot.rg (0.0f, 5.0f);
  return true;
}

void HumanChat::Poll (Bot &bot) {
  if (cv_chat && cv_human_generative_chat && cv_human_chat_catalogue && cv_human_chat_conversation && conversation.sender >= 0) {
    Bot *sender = bots.FindBotByIndex (conversation.sender);
    if (!sender || sender->Ent ()->serialnumber != conversation.serial || sender->pev->netname.str ().hash () != conversation.name ||
        game.Time () > conversation.expires || sender->is_alive_ != conversation.alive || sender->team_ != conversation.team) conversation = Conversation {};
    else if (game.Time () >= conversation.ready) {
      Bot *responder = nullptr; int rank = 10000;
      for (auto &candidate : bots) {
        if (&candidate == sender || candidate.is_alive_ != conversation.alive || candidate.not_started_ || candidate.is_creature_ ||
            (conversation.team_only && candidate.team_ != conversation.team) || !candidate.pending_generated_chat_.empty () ||
            game.Time () < candidate.next_generated_chat_ || candidate.has_progress_bar_ || candidate.is_using_grenade_ ||
            has_flag (candidate.states_,Sense::SeeingEnemy)) continue;
        const int score = candidate.Index () + (std::string (Language (candidate)) == Language (*sender) ? 100 : 0);
        if (score < rank) { responder = &candidate; rank = score; }
      }
      if (responder == &bot) {
        auto topic = conversation.topic;
        if (topic == rbcc::DialogueIntent::Kill) topic = rbcc::DialogueIntent::Praise;
        if (topic == rbcc::DialogueIntent::Dead || topic == rbcc::DialogueIntent::Apology) topic = rbcc::DialogueIntent::Plan;
        if (topic == rbcc::DialogueIntent::Explore) topic = rbcc::DialogueIntent::Rotate;
        if (topic == rbcc::DialogueIntent::TopFragger) topic = rbcc::DialogueIntent::Praise;
        if (topic == rbcc::DialogueIntent::Angry || topic == rbcc::DialogueIntent::Brave || topic == rbcc::DialogueIntent::Ambition || topic == rbcc::DialogueIntent::Boost) topic = rbcc::DialogueIntent::Plan;
        if (topic == rbcc::DialogueIntent::Afraid || topic == rbcc::DialogueIntent::Cautious) topic = rbcc::DialogueIntent::Hold;
        if (ModernDialogue (bot,Chat::NoKeyword,bot.pending_generated_chat_,{},true,topic)) {
          bot.pending_generated_team_ = conversation.team_only;
          bot.pending_generated_time_ = game.Time () + 1.0f + static_cast<float> (bot.pending_generated_chat_.size ()) * 0.02f;
          bot.next_generated_chat_ = bot.pending_generated_time_ + cv_human_chat_interval.As<float> ();
          conversation = Conversation {};
        }
      }
    }
  }
  if (cv_chat && cv_human_generative_chat && game.Time () >= chat_ambient_next) {
    Bot *speaker = nullptr;
    int best = kGameMaxPlayers + 1;
    for (auto &candidate : bots) {
      if (!candidate.is_alive_ || candidate.not_started_ || candidate.is_creature_ ||
          !candidate.pending_generated_chat_.empty () || game.Time () < candidate.next_generated_chat_ ||
          candidate.has_progress_bar_ || candidate.is_using_grenade_ || has_flag (candidate.states_, Sense::SeeingEnemy)) continue;
      const int order = (candidate.Index () - chat_ambient_last + kGameMaxPlayers - 1) % kGameMaxPlayers;
      if (order < best) { speaker = &candidate; best = order; }
    }
    if (speaker == &bot && Queue (bot, bot.generated_greeted_ ? Chat::NoKeyword : Chat::Hello, false)) {
      bot.generated_greeted_ = true;
      chat_ambient_last = bot.Index (); chat_ambient_next = game.Time () + (HumanArena::Enabled () ? bot.rg (6.0f,10.0f) : bot.rg (10.0f, 18.0f));
    }
  }
  if (bot.pending_generated_chat_.empty ()) return;
  if (cv_human_chat_catalogue) {
    const auto &state = DialogueState (bot);
    if (state.generated == rbcc::DialogueIntent::TopFragger && !HumanArena::Top (bot)) { bot.pending_generated_chat_.clear (); return; }
    if (state.reply) {
      const auto source = bots.FindBotByIndex (state.reply_sender);
      if (state.scope_alive != bot.is_alive_ || state.scope_team != bot.team_ || !source ||
          source->Ent ()->serialnumber != state.reply_serial || source->is_alive_ != state.scope_alive ||
          source->pev->netname.str ().hash () != state.reply_name || source->team_ != state.reply_team) {
        bot.pending_generated_chat_.clear (); return;
      }
    }
  }
  if (!cv_chat || !cv_human_generative_chat || bot.is_creature_ || game.Time () > bot.pending_generated_time_ + 10.0f) {
    bot.pending_generated_chat_.clear (); return;
  }
  if (game.Time () < bot.pending_generated_time_ || game.Time () < chat_global_next) return;
  if (bot.is_alive_ && (bot.has_progress_bar_ || bot.is_using_grenade_ || has_flag (bot.states_, Sense::SeeingEnemy))) return;
  // Direct delivery avoids the old single mutable chat buffer queue and prevents
  // a generated bot line from starting another bot-to-bot reply cascade.
  bot.SendToChatDirect (bot.pending_generated_chat_, bot.pending_generated_team_);
  if (cv_human_chat_catalogue) {
    const auto &s = DialogueState (bot);
    if (s.reply) ++conversation_replies;
    if (s.may_reply && !s.reply && cv_human_chat_conversation && conversation.sender < 0) {
      conversation.sender = bot.Index (); conversation.serial = bot.Ent ()->serialnumber; conversation.name = bot.pev->netname.str ().hash ();
      conversation.team = bot.team_; conversation.team_only = bot.pending_generated_team_; conversation.alive = bot.is_alive_;
      conversation.topic = s.generated; conversation.ready = game.Time () + 1.5f; conversation.expires = game.Time () + 18.0f;
      chat_ambient_next = std::max (chat_ambient_next,game.Time () + (HumanArena::Enabled () ? 8.0f : 12.0f));
    }
  }
  bot.pending_generated_chat_.clear ();
  bot.last_chat_timer_.start (); bots.MarkLastChatTime ();
  chat_global_next = game.Time () + (HumanArena::Enabled () ? cv_final_chat_gap.As<float> () : 3.0f);
}
void HumanChat::Status () {
  game.Print ("[Dialogue] catalogue=%d languages=%u fragments=%u generated=%u replies=%u conversation=%d mode=%s",
    cv_human_chat_catalogue.As<int> (),rbcc::dialogueLanguageCount (),rbcc::dialogueFragmentCount (),generated_lines,conversation_replies,
    cv_human_chat_conversation.As<int> (),cv_human_chat_language.ptr->string);
  for (const auto &b : bots) game.Print ("[Dialogue] %s profile=%s language=%s",b.pev->netname.chars (),rbcc::profileCountry (b.pev->netname.chars ()),Language (b));
}
#if defined(YSTL_TESTS)
unsigned HumanChat::TestReplies () { return conversation_replies; }
bool HumanChat::TestConversation () { return conversation.sender >= 0; }
#endif
} // namespace bot
