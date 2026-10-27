// AiBo 4.3.3: researched signature roles and historical annual HLTV peaks.
// GPL-2.0-or-later. Sources and interpretation: PRO-PROFILES-4.3.3-ID.md.
#pragma once
#include <array>
#include <string>
#include <string_view>
namespace rbcc {
enum class ProWeapon {
  Any,
  Rifle,
  AWP
};
struct ProProfile {
  const char *name;
  ProWeapon weapon;
  int rank, year;
};
inline constexpr std::array<ProProfile, 42> proProfiles {
  { { "NEO", ProWeapon::Rifle, 1, 2011 }, { "f0rest", ProWeapon::Rifle, 2, 2013 }, { "GeT_RiGhT", ProWeapon::Rifle, 1, 2013 },
   { "markeloff", ProWeapon::AWP, 1, 2010 }, { "trace", ProWeapon::Rifle, 3, 2010 }, { "starix", ProWeapon::Rifle, 4, 2010 },
   { "Edward", ProWeapon::Rifle, 5, 2010 }, { "zonic", ProWeapon::Rifle, 8, 2010 }, { "moddii", ProWeapon::Rifle, 9, 2010 },
   { "Delpan", ProWeapon::AWP, 9, 2011 }, { "TaZ", ProWeapon::Rifle, 6, 2011 }, { "Friis", ProWeapon::AWP, 7, 2011 },
   { "karrigan", ProWeapon::AWP, 8, 2011 }, { "Gux", ProWeapon::Rifle, 10, 2011 }, { "shox", ProWeapon::Rifle, 3, 2013 },
   { "Dosia", ProWeapon::Rifle, 4, 2013 }, { "NBK", ProWeapon::Rifle, 5, 2013 }, { "Xizt", ProWeapon::Rifle, 6, 2013 },
   { "ScreaM", ProWeapon::Rifle, 7, 2013 }, { "JW", ProWeapon::AWP, 5, 2014 }, { "Hiko", ProWeapon::Rifle, 9, 2013 },
   { "GuardiaN", ProWeapon::AWP, 2, 2015 }, { "flusha", ProWeapon::Rifle, 2, 2014 }, { "pasha", ProWeapon::AWP, 3, 2014 },
   { "Snax", ProWeapon::Rifle, 4, 2014 }, { "kennyS", ProWeapon::AWP, 6, 2014 }, { "KRiMZ", ProWeapon::Rifle, 7, 2015 },
   { "Happy", ProWeapon::Rifle, 8, 2015 }, { "BnTeT", ProWeapon::Rifle, 0, 0 }, { "xccurate", ProWeapon::AWP, 0, 0 },
   { "somebody", ProWeapon::Rifle, 0, 0 }, { "XigN", ProWeapon::Rifle, 0, 0 }, { "Laz", ProWeapon::Rifle, 0, 0 },
   { "ISSAA", ProWeapon::Rifle, 0, 0 }, { "BOROS", ProWeapon::Rifle, 0, 0 }, { "FalleN", ProWeapon::AWP, 2, 2016 },
   { "coldzera", ProWeapon::Rifle, 1, 2016 }, { "XANTARES", ProWeapon::Rifle, 14, 2025 }, { "gob b", ProWeapon::Rifle, 0, 0 },
   { "EliGE", ProWeapon::Rifle, 4, 2019 }, { "Stewie2K", ProWeapon::Rifle, 0, 0 }, { "sh1ro", ProWeapon::AWP, 3, 2022 } }
};
inline std::string proKey (std::string_view input) {
  const auto duplicate = input.rfind (" (");
  if (duplicate != std::string_view::npos && input.back () == ')')
    input = input.substr (0, duplicate);
  std::string out;
  for (const unsigned char c : input) {
    if (c >= 'A' && c <= 'Z')
      out += static_cast<char> (c + ('a' - 'A'));
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
      out += static_cast<char> (c);
  }
  if (out == "forest")
    out = "f0rest";
  if (out == "pashabiceps")
    out = "pasha";
  return out;
}
inline const ProProfile *proProfile (std::string_view name) {
  const auto key = proKey (name);
  for (const auto &p : proProfiles)
    if (key == proKey (p.name))
      return &p;
  return nullptr;
}
inline float rankPower (int rank) {
  if (rank < 1 || rank > 20)
    return 0.5f;
  return 1.0f - static_cast<float> (rank - 1) * 0.025f;
}
}
