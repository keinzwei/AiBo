AiBo 4.3.3 — Artificial Intelligence Bot

AiBo creator and project maintainer: K9_einzwei (GitHub: keinzwei).

The combined bot is distributed under GPL-2.0-or-later; see COPYING.GPL-2.0.
It combines the YaPB upstream code (commit 7985904584a0755b216e17cc5de8c5fd28953895,
Unlicense) with the supplied RealBot Human Copycat core (GPL-2.0-or-later).
Original authorship and component notices remain in their source files.
UNLICENSE applies to the original upstream code, not the combined fork.
AiBo is an independent fork, not an official upstream release.

Vendored linkage, ystl, printf, mbedTLS and TF-PSA-Crypto retain their original
licenses and notices in ext/. GNU runtime libraries linked into Windows binaries
retain their license and GCC Runtime Library Exception; see licenses/.
Supplied Metamod, AMX Mod X and Semiclip components retain their own licenses.
They are runtime components, not required to compile the bot DLL itself.

The unmodified de_dust2.graph fallback from https://github.com/yapb/graph is
CC BY-SA 4.0. Its author marker is $_Vladislav. Full license, upstream credits,
exact source URL and SHA-256 are in cfg/addons/AiBo/licenses/. Existing installed
.graph files are preserved. Native-compatible graph/experience data and Human
Copycat wire formats retain their identifiers for compatibility.

Public player names, signature roles and historical HLTV annual rankings are
references only and do not imply endorsement. Ranking-to-ability conversion is
AiBo's simulation design, not an official HLTV formula. No player photos, voices,
private recordings or trained models are bundled. Dialogue uses local authored
procedural grammar. The two supplied autobuy/listenserver files are unchanged.

Source distribution contains the active release and its tests only. Archived
release notes, old patches, old implementations and build outputs are excluded.
The synthetic numeric aim regression fixture contains no personal player data.
