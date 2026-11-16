# Changelog

## 4.3.5

- **50% AWP / 50% AK-47 & M4A1 Team Loadout Composition:** Balanced primary weapon quota (`aibo_human_awp_percent "50"`) so 50% of each bot team equips AWP and the remaining 50% equips AK-47 (Terrorist) or M4A1 (CT), paired with 100% Deagle secondary priority (`aibo_human_deagle_percent "100"`).
- **Pro Primary-to-Secondary Weapon Switching (`ProWeaponSwitchController`):**
  - **AWP Bots:** Instant post-shot quick-switch (`AWP -> Deagle`), close-quarters defense switch, and pro mobility travel switching (`AWP -> Deagle` at 250 u/s while walking/running to spot, then automatically switching back `Deagle -> AWP` upon reaching the sniper spot).
  - **AK-47 / M4A1 Bots:** Automatic primary-to-secondary (`AK-47/M4A1 -> Deagle`) switching when the rifle magazine is empty or critically low mid-gunfight, after sustained burst/spray recoil recovery, or when a primary reload is interrupted by enemy sight.
- **Free-Roaming CT Exploration & Patrol Routes:** Restored free-roaming CT map exploration, hunting, and Dust2 patrol routing so CTs dynamically roam the map instead of camping passively at bombsites pre-plant, while converging immediately once C4 is planted.
- **Minimum 16 Initial Bots Before Internet Simulation (`kInternetModeMinInitialBots = 16`):** Rapidly spawns at least 16 bots at match start before the 10-30s Internet Mode join/leave simulation activates.

## 4.3.3

- CT searches through Dust2 Tunnel, T-Spawn and Pit, with objective/contact priorities and rotating participation.
- Forty-two player-inspired weapon roles and documented historical ranking-based weights.
- Human Aim defaults at level 100; adaptive mode at level -1.
- Hybrid GRAPH/human-route/geometry navigation, six local learning mechanisms, persistent memory and dialogue in 16 languages.
- Windows installer with folder checks, plugin configuration merging, numbered backups and rollback handling.
