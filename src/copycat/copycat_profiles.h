// AiBo 4.3.5: researched signature roles and historical annual HLTV peaks.
// Enhanced for CS 1.6 Legends: f0rest, NEO, GeT_RiGhT, markeloff, HeatoN, Potti, SpawN, elemeNt, Edward, TaZ, gob b, cArn.
// Balanced 50% AWP / 50% Rifle (AK-47/M4A1) Pro Roster.
// GPL-2.0-or-later.
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
inline constexpr std::array<ProProfile, 53> proProfiles {
  { { "NEO", ProWeapon::Rifle, 1, 2011 }, { "f0rest", ProWeapon::Rifle, 2, 2013 }, { "GeT_RiGhT", ProWeapon::Rifle, 1, 2013 },
   { "markeloff", ProWeapon::AWP, 1, 2010 }, { "HeatoN", ProWeapon::Rifle, 1, 2001 }, { "Potti", ProWeapon::Rifle, 1, 2002 },
   { "SpawN", ProWeapon::AWP, 1, 2004 }, { "elemeNt", ProWeapon::Rifle, 1, 2003 }, { "cArn", ProWeapon::AWP, 1, 2006 },
   { "gob b", ProWeapon::Rifle, 1, 2007 }, { "Edward", ProWeapon::Rifle, 5, 2010 }, { "TaZ", ProWeapon::Rifle, 6, 2011 },
   { "trace", ProWeapon::AWP, 3, 2010 }, { "starix", ProWeapon::AWP, 4, 2010 }, { "zonic", ProWeapon::AWP, 8, 2010 },
   { "moddii", ProWeapon::Rifle, 9, 2010 }, { "Delpan", ProWeapon::AWP, 9, 2011 }, { "Friis", ProWeapon::AWP, 7, 2011 },
   { "karrigan", ProWeapon::AWP, 8, 2011 }, { "Gux", ProWeapon::AWP, 10, 2011 }, { "walle", ProWeapon::AWP, 2, 2005 },
   { "dsn", ProWeapon::AWP, 2, 2006 }, { "ave", ProWeapon::AWP, 2, 2007 }, { "kuben", ProWeapon::Rifle, 5, 2007 },
   { "loord", ProWeapon::Rifle, 5, 2008 }, { "Sunde", ProWeapon::AWP, 3, 2008 }, { "shox", ProWeapon::AWP, 3, 2013 },
   { "Dosia", ProWeapon::Rifle, 4, 2013 }, { "NBK", ProWeapon::Rifle, 5, 2013 }, { "Xizt", ProWeapon::Rifle, 6, 2013 },
   { "ScreaM", ProWeapon::Rifle, 7, 2013 }, { "JW", ProWeapon::AWP, 5, 2014 }, { "Hiko", ProWeapon::Rifle, 9, 2013 },
   { "GuardiaN", ProWeapon::AWP, 2, 2015 }, { "flusha", ProWeapon::Rifle, 2, 2014 }, { "pasha", ProWeapon::AWP, 3, 2014 },
   { "Snax", ProWeapon::AWP, 4, 2014 }, { "kennyS", ProWeapon::AWP, 6, 2014 }, { "KRiMZ", ProWeapon::Rifle, 7, 2015 },
   { "Happy", ProWeapon::AWP, 8, 2015 }, { "BnTeT", ProWeapon::Rifle, 0, 0 }, { "xccurate", ProWeapon::AWP, 0, 0 },
   { "somebody", ProWeapon::Rifle, 0, 0 }, { "XigN", ProWeapon::AWP, 0, 0 }, { "Laz", ProWeapon::AWP, 0, 0 },
   { "ISSAA", ProWeapon::Rifle, 0, 0 }, { "BOROS", ProWeapon::Rifle, 0, 0 }, { "FalleN", ProWeapon::AWP, 2, 2016 },
   { "coldzera", ProWeapon::AWP, 1, 2016 }, { "XANTARES", ProWeapon::Rifle, 14, 2025 }, { "EliGE", ProWeapon::Rifle, 4, 2019 },
   { "Stewie2K", ProWeapon::AWP, 0, 0 }, { "sh1ro", ProWeapon::AWP, 3, 2022 } }
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
  if (out == "gobb" || out == "gob_b")
    out = "gobb";
  if (out == "element")
    out = "element";
  if (out == "carn")
    out = "carn";
  if (out == "getright" || out == "get_right")
    out = "getright";
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
