// YaPB Human Copycat 4.2.7. SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

namespace bot {
// Bounded local sentence generation from intents, grammar, state and bot style.
// No network calls, model downloads, complete-line chat bank, or hidden enemy data.
class HumanChat final {
public:
  static bool Generate (Bot &bot, Chat event, ystl::String &out, ystl::StringRef incoming = {});
  static bool Queue (Bot &bot, Chat event, bool team, ystl::StringRef incoming = {});
  static void Poll (Bot &bot);
  static void Reset ();
  static void NewRound ();
  static void Status ();
  static const char *Language (const Bot &bot);
#if defined(YSTL_TESTS)
  static unsigned TestReplies ();
  static bool TestConversation ();
#endif
};
} // namespace bot
