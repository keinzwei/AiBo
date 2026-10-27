// YaPB Human Copycat 4.2.7. SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "../src/copycat/copycat_profiles.h"

namespace bot::human_policy {
inline constexpr ystl::StringRef LegendNames[] = {
  "NEO", "f0rest", "GeT_RiGhT", "markeloff", "trace", "starix", "Edward", "zonic", "moddii", "Delpan", "TaZ", "Friis", "karrigan", "Gux",
  "shox", "Dosia", "NBK", "Xizt", "ScreaM", "JW", "Hiko", "GuardiaN", "flusha", "pasha", "Snax", "kennyS", "KRiMZ", "Happy" };
inline constexpr ystl::StringRef InternationalNames[] = {
  "BnTeT", "xccurate", "somebody", "XigN", "Laz", "ISSAA", "BOROS", "FalleN", "coldzera", "XANTARES", "gob b", "EliGE", "Stewie2K", "sh1ro" };

inline const rbcc::ProProfile *Profile (const edict_t *ent) {
  return cv_human_pro_profiles && ent ? rbcc::proProfile (ent->v.netname.chars ()) : nullptr;
}
inline bool RifleProfile (const edict_t *ent) { const auto p = Profile (ent); return p && p->weapon == rbcc::ProWeapon::Rifle; }
inline bool AwpProfile (const edict_t *ent) { const auto p = Profile (ent); return p && p->weapon == rbcc::ProWeapon::AWP; }
inline float RankPower (const edict_t *ent) {
  const auto p = cv_human_hltv_skill && ent ? rbcc::proProfile (ent->v.netname.chars ()) : nullptr;
  return p ? rbcc::rankPower (p->rank) : 1.0f;
}

// Deterministic fractional rounding varies 5-bot teams between 1 and 2 AWPs
// at 25%, while always retaining a rifle majority for teams of 3 or more.
constexpr int SniperQuota (int count, int percent, unsigned round) {
  if (count <= 0 || percent <= 0) return 0;
  const int target = (count * ystl::clamp (percent, 0, 100) + static_cast<int> ((round * 37u) % 100u)) / 100;
  return ystl::min (target, count >= 3 ? (count - 1) / 2 : count);
}

// Immutable commands: gamedef.cfg must not redirect an allowed ID to a
// different menu slot, alias, or weapon classname.
constexpr ystl::StringRef BuyAlias (Weapon id) {
  switch (id) {
  case Weapon::AK47: return "ak47";
  case Weapon::M4A1: return "m4a1";
  case Weapon::AWP: return "awp";
  case Weapon::Deagle: return "deagle";
  case Weapon::USP: return "usp";
  case Weapon::Glock18: return "glock";
  default: return "";
  }
}

// This release's loadout is an invariant, independent of difficulty/config files.
constexpr bool Firearm (Weapon id) {
  return id == Weapon::AWP || id == Weapon::AK47 || id == Weapon::M4A1 ||
    id == Weapon::Deagle || id == Weapon::USP || id == Weapon::Glock18;
}
constexpr bool Usable (Weapon id) {
  return Firearm (id) || id == Weapon::Knife || id == Weapon::C4 ||
    id == Weapon::Flashbang || id == Weapon::Smoke || id == Weapon::Explosive;
}
constexpr bool Purchasable (Weapon id) {
  return Firearm (id) || id == Weapon::Flashbang || id == Weapon::Smoke ||
    id == Weapon::Explosive || id == Weapon::Armor || id == Weapon::ArmorHelm || id == Weapon::Defuser;
}
constexpr uint32_t PrimaryMask = (1u << 18u) | (1u << 22u) | (1u << 28u);
constexpr uint32_t PistolMask = (1u << 16u) | (1u << 17u) | (1u << 26u);
constexpr WeaponTeam DefaultTeam (Weapon id) {
  if (id == Weapon::AK47) return WeaponTeam::Terrorist;
  if (id == Weapon::M4A1) return WeaponTeam::CT;
  return Firearm (id) ? WeaponTeam::Both : WeaponTeam::None;
}

// Also used by the touch hook: forbidden weapons cannot be acquired accidentally.
inline bool BlockPickup (edict_t *ent) {
  if (game.IsNullEntity (ent)) return false;
  const auto classname = ent->v.classname.str ();
  if (classname.starts_with ("weapon_shield")) return true;
  const bool weapon = classname.starts_with ("weaponbox") || classname.starts_with ("armoury_entity") ||
    classname.starts_with ("weapon_") || classname.starts_with ("csdm");
  if (!weapon) return false;
  const auto model = ent->v.model.str ();
  for (const auto &info : conf.GetWeapons ()) {
    if (!info.model.empty () && model.ends_with (info.model)) return !Usable (info.id);
  }
  // Equipment/ammo boxes and C4 remain available. Unknown weapon-box models
  // fail closed instead of becoming an alternate route around the loadout.
  constexpr ystl::StringRef equipment[] = { "backpack.mdl", "hegrenade.mdl", "flashbang.mdl", "smokegrenade.mdl",
    "medkit.mdl", "kevlar.mdl", "battery.mdl", "assault.mdl", "9mmarclip.mdl", "shotbox.mdl",
    "9mmclip.mdl", "crossbow_clip.mdl", "chainammo.mdl", "357ammobox.mdl" };
  for (const auto &suffix : equipment) if (model.ends_with (suffix)) return false;
  return true;
}
} // namespace bot::human_policy
