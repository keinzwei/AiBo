# AiBo — Artificial Intelligence Bot

**Human-inspired Counter-Strike 1.6 bots, built on YaPB and extended with local learning.**

[![Version](https://img.shields.io/badge/version-4.3.3-blue)](https://github.com/keinzwei/AiBo/releases/tag/v4.3.3)
[![Platform](https://img.shields.io/badge/release-Windows%20x86-0078D4)](https://github.com/keinzwei/AiBo/releases)
[![License](https://img.shields.io/badge/license-GPL--2.0--or--later-green)](LICENSE)

[Download installer](https://github.com/keinzwei/AiBo/releases/tag/v4.3.3) · [Bahasa Indonesia](README.id.md) · [Build guide](docs/human-copycat/BUILD.md) · [Credits](CREDITS.md)

AiBo combines YaPB's navigation and tactical foundation with the Human Copycat extension: controlled aim, observed human movement, lightweight online learning, geometry exploration and persistent match experience. It runs locally inside the game, without a cloud AI service, GPU, downloaded language model or API subscription.

This repository publishes **AiBo 4.3.3**, an independent community fork, not an official YaPB release.

## Features

| Area | Implementation |
| --- | --- |
| Hybrid navigation | Native `.GRAPH` navigation, observed human routes and geometry exploration with floor, clearance, connection and progress checks. This version still uses graphs. |
| Dust2 CT patrols | Situation-dependent searches through Tunnel, T-Spawn and Pit, with rotating patrol participation. Some CTs remain on objectives; enemy contact and urgent bomb tasks take priority. |
| Human Aim | Reaction delay, bounded turning, aim variation, recoil handling, tapping, bursts, spray control, counter-strafe, preaim and scoped AWP behavior. Hits remain subject to the game engine. |
| 42 player-inspired profiles | AWP/rifle preferences and simulated ability weights based on documented historical HLTV rankings. NEO/f0rest favor rifles; GuardiaN/kennyS favor affordable AWP purchases. |
| Local learning | Tabular reinforcement learning, human demonstration priors, bot-versus-bot updates, persistent experience, observed opponent adaptation and shared team formation learning. |
| Tactical variety | Opening formations, strategy variation, smoke around objectives, physical boosting where player collision permits, and location/strategy memory. |
| Personalities and dialogue | Simulated competitive moods and local procedural conversations in 16 profile languages, using authored grammar. |
| Persistent data | Validated files for demonstrations, routes, learning, exploration, strategy memory and aim calibration. No personal gameplay dataset is bundled. |
| Windows installer | Game-folder checks, plugin configuration merging, numbered `.bak` backups, rollback handling and preservation of custom graphs. |
| Complete source | C++ code, vendored dependencies, configurations, installer code, tests and build instructions. |

## Install on Windows

1. Download **`AiBo-4.3.3-Setup.exe`** from [Releases](https://github.com/keinzwei/AiBo/releases/tag/v4.3.3).
2. Close Counter-Strike and HLDS.
3. Run the installer and select your Counter-Strike folder or its `cstrike` folder.
4. Start a map and check the console:

   ```text
   aibo version
   aibo_human_status
   aibo_human_aim_status
   aibo_learning_status
   aibo_final_status
   ```

The full Windows package supplies the runtime components used by the installer. The game itself is not included. Replaced files are backed up as `.bak`, `.bak.1`, and so on. The supplied `autobuy.txt` and `listenserver.cfg` are installed unchanged; review their settings if you maintain a customized server.

The DLL is installed at `cstrike/addons/AiBo/bin/AiBo.dll`. Its Metamod entry is:

```text
win32 addons/AiBo/bin/AiBo.dll
```

Use the installer for its backup and configuration-merge behavior. A 32-slot server allows up to 31 bots alongside one human. Physical boosting needs compatible collision/Semiclip settings.

## Settings

Edit `cstrike/addons/AiBo/conf/AiBo.cfg` for persistent changes.

| Setting | Default | Purpose |
| --- | ---: | --- |
| `aibo_human_aim_level` | `100` | Global Pro aim ceiling; `-1` selects adaptive mode. |
| `aibo_human_aim_skill` | `100` | Base aim skill. |
| `aibo_human_reaction_skill` | `100` | Base reaction skill. |
| `aibo_human_pro_profiles` | `1` | Profile weapon roles. |
| `aibo_human_hltv_skill` | `1` | Profile ability weighting. |
| `aibo_human_ct_patrol` | `1` | CT search patrols on Dust2. |
| `aibo_learning` | `1` | Local learning system. |

Player names and rankings are simulation references, without endorsement or measured equivalence to real players. The ranking-to-ability conversion is AiBo's own design.

## Learning

- **Reinforcement learning:** movement decisions receive bounded rewards from combat and round outcomes.
- **Imitation learning:** eligible human inputs contribute action priors and reusable demonstrations/routes.
- **Self-play:** ordinary bot-versus-bot matches provide online updates while the game runs.
- **Lifelong learning:** map/environment-specific experience is saved and restored between sessions.
- **Continual adaptation:** visible opponent behavior updates approach/hold estimates.
- **Multi-agent learning:** teammates share experience and adjust a small set of opening formations.

These mechanisms use local tables and summaries, not neural networks. Self-play does not create an offline training farm. Learning quality depends on experience and does not guarantee a higher win rate. See the [learning guide](LEARNING-ID.md) for controls and limits.

## Build

Requirements: **CMake 3.22+, a C++20 compiler, Python 3 and Ninja or Make**. Dependencies are vendored in `ext/`; this snapshot needs no submodule download.

```sh
cmake -S . -B build/test -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DAIBO_NATIVE=ON -DAIBO_64BIT=ON \
  -DAIBO_BUILD_TESTS=ON -DAIBO_WITH_TLS=OFF \
  -DAIBO_WITH_AMXX=OFF -DAIBO_LTO=OFF
cmake --build build/test --parallel 4
ctest --test-dir build/test --output-on-failure --timeout 90 --parallel 2
```

The [build guide](docs/human-copycat/BUILD.md) covers Windows x86, MinGW, MSVC and the installer. Source snapshots do not include compiled third-party runtime DLLs; the full Windows release package supplies the installer payload. Check the Actions tab for the actual status of the included CI workflow.

## Documentation

| Guide | Contents |
| --- | --- |
| [Release notes](RELEASE-4.3.3-ID.md) | Changes and defaults. |
| [Human Copycat](HUMAN_COPYCAT.md) | Integration and persistent formats. |
| [Human Aim](HUMAN-AIM-4.3.3-ID.md) | Aim, perception and movement. |
| [Learning](LEARNING-ID.md) | Six local learning mechanisms. |
| [Exploration and dialogue](EXPLORE-CHAT-ID.md) | Geometry and conversations. |
| [Player profiles](PRO-PROFILES-4.3.3-ID.md) | Roles, rankings, sources and weighting. |
| [Final systems](FINAL-GUIDE-ID.md) | Strategy memory and personalities. |
| [Validation](docs/VALIDATION.md) | Test evidence and limits. |
| [Build](docs/human-copycat/BUILD.md) | Bot and installer compilation. |

## Validation and limitations

The supplied release records **293 passing code tests and 11 passing installer scenarios** on Linux, using a fake-engine harness and native installer core. These do not establish real-game pathfinding success, Windows installer UI behavior, compatibility with every mod or performance against professional players. The original Windows installer and DLL are distributed as supplied, with checksums.

Routes must be physically reachable; geometry checks do not guarantee every map corner can be reached. Wallbang uses remembered legitimate contact rather than continuously refreshed hidden enemy coordinates. Game physics, spread, cooldown and server rules determine outcomes.

## Credits and license

Published by [keinzwei](https://github.com/keinzwei). AiBo builds on **YaPB, jeefo and the YaPB contributors**, with **PODBot / Count Floyd** ancestry and the supplied **RealBot Human Copycat** extension. Graph authors and all dependency/runtime contributors retain their credits.

The combined fork is **GPL-2.0-or-later**, as declared by the supplied notices. Components retain their own terms. Read [LICENSE](LICENSE), [NOTICE-AiBo.md](NOTICE-AiBo.md), [CREDITS.md](CREDITS.md) and notices under `ext/` and `cfg/addons/AiBo/licenses/`.

Counter-Strike and Half-Life belong to their respective rights holders. AiBo is an independent community project.

Bug reports and contributions are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). Please report fork-specific issues here.
