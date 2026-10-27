// AiBo 4.3.3 transactional installer engine. GPL-2.0-or-later.
#include "install_core.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <tlhelp32.h>
#endif
namespace aibo_setup {
namespace fs = std::filesystem;
namespace {
constexpr const char *kDll = "addons/AiBo/bin/AiBo.dll";
constexpr const char *kIni = "addons/metamod/plugins.ini";
std::string Lower (std::string s) {
  for (auto &c : s)
    c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
  return s;
}
std::string Read (const fs::path &p) {
  std::ifstream f (p, std::ios::binary);
  if (!f)
    throw std::runtime_error ("Tidak dapat membaca file payload/target.");
  std::ostringstream b;
  b << f.rdbuf ();
  if (f.bad ())
    throw std::runtime_error ("Pembacaan file gagal.");
  return b.str ();
}
std::vector<std::string> Lines (const std::string &s) {
  if (s.size () > 4 * 1024 * 1024)
    throw std::runtime_error ("Konfigurasi terlalu besar.");
  if (s.find ('\0') != std::string::npos)
    throw std::runtime_error ("Konfigurasi harus ANSI/UTF-8, bukan UTF-16/biner.");
  std::vector<std::string> out;
  std::istringstream in (s);
  std::string line;
  while (std::getline (in, line)) {
    if (!line.empty () && line.back () == '\r')
      line.pop_back ();
    out.push_back (line);
  }
  if (!out.empty () && out[0].starts_with ("\xef\xbb\xbf"))
    out[0].erase (0, 3);
  return out;
}
std::vector<std::string> Tokens (const std::string &s) {
  std::vector<std::string> out;
  std::string word;
  bool quote = false;
  for (size_t i = 0; i < s.size (); ++i) {
    const char c = s[i];
    if (!quote && (c == ';' || (c == '/' && i + 1 < s.size () && s[i + 1] == '/')))
      break;
    if (c == '"') {
      quote = !quote;
      continue;
    }
    if (!quote && std::isspace (static_cast<unsigned char> (c))) {
      if (!word.empty ()) {
        out.push_back (word);
        word.clear ();
      }
    }
    else
      word += c;
  }
  if (!word.empty ())
    out.push_back (word);
  return out;
}
std::string Slash (std::string s) {
  std::replace (s.begin (), s.end (), '\\', '/');
  return s;
}
bool BotModule (const std::string &name) {
  const auto path = Slash (Lower (name));
  const auto slash = path.find_last_of ('/');
  const auto file = path.substr (slash == std::string::npos ? 0 : slash + 1);
  return (file.starts_with ("aibo") || file.starts_with ("yapb") || file.starts_with ("realbot")) &&
         (file.ends_with (".dll") || file.ends_with (".so") || file.ends_with (".dylib"));
}
bool Amxx (const std::string &s) {
  const auto n = Lower (Slash (s));
  return n.ends_with ("amxmodx_mm.dll");
}
std::string Join (const std::vector<std::string> &lines, const std::string &original) {
  const auto nl = original.find ("\r\n") != std::string::npos ? "\r\n" : "\n";
  std::string out;
  for (const auto &line : lines)
    out += line + nl;
  return out;
}
void SafeRelative (const fs::path &p) {
  if (p.empty () || p.is_absolute () || p.has_root_name ())
    throw std::runtime_error ("Path payload tidak aman.");
  for (const auto &part : p)
    if (part == ".." || part == "." || part.string ().find (':') != std::string::npos)
      throw std::runtime_error ("Path payload tidak aman.");
}
void NoLinks (const fs::path &p) {
  fs::path walked;
  for (const auto &part : fs::absolute (p)) {
    walked /= part;
    if (fs::is_symlink (fs::symlink_status (walked)))
      throw std::runtime_error ("Folder target tidak boleh symlink/junction.");
#ifdef _WIN32
    const auto attr = GetFileAttributesW (walked.c_str ());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
      throw std::runtime_error ("Folder target tidak boleh junction/reparse point.");
#endif
  }
}
void RunningGame () {
#ifdef _WIN32
  const HANDLE snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE)
    throw std::runtime_error ("Tidak dapat memeriksa proses game. Tutup CS/HLDS lalu ulangi.");
  PROCESSENTRY32W e {};
  e.dwSize = sizeof e;
  bool running = false;
  if (Process32FirstW (snapshot, &e))
    do {
      std::wstring name = e.szExeFile;
      for (auto &c : name)
        c = static_cast<wchar_t> (towlower (c));
      if (name == L"hl.exe" || name == L"hlds.exe" || name == L"cstrike.exe")
        running = true;
    } while (Process32NextW (snapshot, &e));
  CloseHandle (snapshot);
  if (running)
    throw std::runtime_error ("Tutup Counter-Strike dan HLDS sebelum memasang AiBo.");
#endif
}
void Write (const fs::path &p, const std::string &bytes) {
  fs::create_directories (p.parent_path ());
  std::ofstream f (p, std::ios::binary | std::ios::trunc);
  if (!f)
    throw std::runtime_error ("Folder/file tidak dapat ditulis. Jalankan installer sebagai administrator.");
  f.write (bytes.data (), static_cast<std::streamsize> (bytes.size ()));
  f.close ();
  if (!f)
    throw std::runtime_error ("Gagal menulis file; periksa ruang disk dan izin folder.");
}
void Replace (const fs::path &from, const fs::path &to) {
#ifdef _WIN32
  auto winpath = [] (const fs::path &p) {
    auto value = fs::absolute (p).lexically_normal ().wstring ();
    std::replace (value.begin (), value.end (), L'/', L'\\');
    if (value.starts_with (L"\\\\?\\"))
      return value;
    if (value.starts_with (L"\\\\"))
      return std::wstring (L"\\\\?\\UNC\\") + value.substr (2);
    return std::wstring (L"\\\\?\\") + value;
  };
  const auto source = winpath (from), target = winpath (to);
  const DWORD attributes = GetFileAttributesW (target.c_str ());
  const bool readonly = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY);
  if (readonly)
    SetFileAttributesW (target.c_str (), attributes & ~FILE_ATTRIBUTE_READONLY);
  if (!MoveFileExW (source.c_str (), target.c_str (), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH)) {
    const DWORD code = GetLastError ();
    if (readonly)
      SetFileAttributesW (target.c_str (), attributes);
    const auto name = to.u8string ();
    throw std::runtime_error ("Gagal mengganti " + std::string (name.begin (), name.end ()) + ". Windows error=" + std::to_string (code) +
                              ". Tutup game dan jalankan installer sebagai administrator.");
  }
#else
  fs::rename (from, to);
#endif
}
uint16_t U16 (const std::string &s, size_t n) {
  if (n + 2 > s.size ())
    throw std::runtime_error ("Header DLL terpotong.");
  return static_cast<unsigned char> (s[n]) | static_cast<uint16_t> (static_cast<unsigned char> (s[n + 1])) << 8;
}
uint32_t U32 (const std::string &s, size_t n) {
  return U16 (s, n) | static_cast<uint32_t> (U16 (s, n + 2)) << 16;
}
void CheckDll (const std::string &s) {
  if (s.size () < 128 || s.substr (0, 2) != "MZ")
    throw std::runtime_error ("AiBo.dll bukan file PE Windows.");
  const size_t pe = U32 (s, 60);
  if (pe > s.size () - 24 || s.substr (pe, 4) != std::string ("PE\0\0", 4) || U16 (s, pe + 4) != 0x14c || !(U16 (s, pe + 22) & 0x2000))
    throw std::runtime_error ("AiBo.dll harus berupa DLL Windows x86 untuk CS 1.6.");
  const std::u16string marker = u"AiBo 4.3.3";
  if (s.find (std::string (reinterpret_cast<const char *> (marker.data ()), marker.size () * 2)) == std::string::npos)
    throw std::runtime_error ("Versi/identitas AiBo.dll tidak sesuai installer 4.3.3.");
}
void CheckSource (const fs::path &payload) {
  CheckDll (Read (payload / kDll));
  unsigned bot_lines = 0;
  for (const auto &line : Lines (Read (payload / kIni))) {
    auto t = Tokens (line);
    if (t.size () < 2 || (Lower (t[0]) != "win32" && Lower (t[0]) != "linux"))
      continue;
    if (BotModule (t[1])) {
      ++bot_lines;
      if (t[0] != "win32" || Slash (t[1]) != kDll)
        throw std::runtime_error ("plugins.ini payload tidak menunjuk ke addons/AiBo/bin/AiBo.dll.");
    }
  }
  if (bot_lines != 1)
    throw std::runtime_error ("plugins.ini harus memuat tepat satu entri AiBo.dll.");
  std::map<std::string, std::string> cfg;
  for (const auto &line : Lines (Read (payload / "addons/AiBo/conf/AiBo.cfg"))) {
    const auto t = Tokens (line);
    if (t.size () >= 2)
      cfg[t[0]] = t[1];
  }
  for (const auto *name : { "human_aim_level", "human_aim_skill", "human_reaction_skill" })
    if (cfg[std::string ("aibo_") + name] != "100")
      throw std::runtime_error ("Default HUMAN AIM payload harus 100.");
  for (const auto *name : { "human_copycat", "human_aim_adaptive", "human_aim_techniques", "human_aim_wallbang", "human_aim_preaim",
         "human_aim_quickswitch", "human_aim_crouchhop", "human_movement", "human_pro_profiles", "human_hltv_skill", "human_ct_patrol" })
    if (cfg[std::string ("aibo_") + name] != "1")
      throw std::runtime_error ("Teknik HUMAN AIM payload belum aktif lengkap.");
}
struct Change {
  fs::path rel;
  std::string bytes;
  bool existed {};
};
}
std::string MergePlugins (const std::string &text) {
  auto lines = Lines (text);
  bool amxx = false, aibo = false;
  for (auto &line : lines) {
    const auto t = Tokens (line);
    if (t.size () < 2 || (Lower (t[0]) != "win32" && Lower (t[0]) != "linux"))
      continue;
    if (BotModule (t[1])) {
      if (Lower (t[0]) == "win32" && Slash (t[1]) == kDll && !aibo)
        aibo = true;
      else
        line = "; disabled by AiBo 4.3.3: " + line;
    }
    else if (Lower (t[0]) == "win32" && Amxx (t[1]))
      amxx = true;
  }
  if (!amxx)
    lines.emplace_back ("win32 addons/amxmodx/dlls/amxmodx_mm.dll");
  if (!aibo)
    lines.emplace_back ("win32 addons/AiBo/bin/AiBo.dll");
  return Join (lines, text);
}
std::string MergeLiblist (const std::string &text) {
  auto lines = Lines (text);
  bool added = false;
  for (auto &line : lines) {
    const auto t = Tokens (line);
    if (t.empty () || Lower (t[0]) != "gamedll")
      continue;
    if (!added) {
      line = "gamedll \"addons/metamod/dlls/metamod.dll\"";
      added = true;
    }
    else
      line = "// replaced by AiBo 4.3.3: " + line;
  }
  if (!added)
    lines.emplace_back ("gamedll \"addons/metamod/dlls/metamod.dll\"");
  return Join (lines, text);
}
Result Install (const fs::path &input, const fs::path &requested, bool check_only, int fail_after) {
  RunningGame ();
  const auto payload = fs::absolute (input);
  NoLinks (payload);
  CheckSource (payload);
  auto game = fs::absolute (requested).lexically_normal ();
  NoLinks (game);
  if (fs::is_regular_file (game / "cstrike/liblist.gam"))
    game /= "cstrike";
  if (Lower (game.filename ().string ()) != "cstrike" || !fs::is_regular_file (game / "liblist.gam"))
    throw std::runtime_error ("Pilih folder Counter-Strike/cstrike yang benar dan memiliki liblist.gam.");
  if (fs::equivalent (game, payload))
    throw std::runtime_error ("Folder payload bukan lokasi game untuk pemasangan.");
  Result result;
  result.game = game;
  std::map<std::string, Change> plan;
  auto add = [&] (const fs::path &rel, const std::string &bytes) {
    SafeRelative (rel);
    NoLinks (game / rel);
    const auto dest = game / rel;
    if (fs::exists (dest) && !fs::is_regular_file (dest))
      throw std::runtime_error ("Path file berbenturan dengan folder.");
    if (fs::is_regular_file (dest) && Read (dest) == bytes)
      return;
    plan[rel.generic_string ()] = { rel, bytes, fs::exists (dest) };
  };
  for (const auto &entry : fs::recursive_directory_iterator (payload)) {
    if (entry.is_symlink ())
      throw std::runtime_error ("Payload tidak boleh berisi link.");
    if (!entry.is_regular_file ())
      continue;
    const auto rel = entry.path ().lexically_relative (payload);
    SafeRelative (rel);
    const auto key = rel.generic_string ();
    if (key == kIni || key == "liblist.gam" || key.find ("/logs/") != std::string::npos)
      continue;
    const bool own = key.starts_with ("addons/AiBo/") || key == "autobuy.txt" || key == "listenserver.cfg";
    NoLinks (game / rel);
    const bool preserve_graph = key.ends_with (".graph") && fs::exists (game / rel);
    if ((!own || preserve_graph) && fs::exists (game / rel)) {
      ++result.preserved;
      continue;
    }
    add (rel, Read (entry.path ()));
  }
  // Native graph and learned data retain their wire formats. Migrate only into
  // absent destinations; original bot files and all personal data stay intact.
  for (const auto *folder : { "data", "graph", "train" }) {
    const auto legacy = game / "addons/yapb" / folder;
    if (!fs::is_directory (legacy))
      continue;
    NoLinks (legacy);
    for (const auto &entry : fs::recursive_directory_iterator (legacy)) {
      if (entry.is_symlink ())
        throw std::runtime_error ("Data migrasi tidak boleh berupa link.");
      if (!entry.is_regular_file ())
        continue;
      const auto relative = entry.path ().lexically_relative (legacy);
      const auto rel = fs::path ("addons/AiBo") / folder / relative;
      if (!fs::exists (game / rel) && !plan.count (rel.generic_string ())) {
        add (rel, Read (entry.path ()));
        ++result.migrated;
      }
    }
  }
  const auto ini = game / kIni;
  const auto new_plugins = MergePlugins (fs::exists (ini) ? Read (ini) : Read (payload / kIni));
  add (kIni, new_plugins);
  add ("liblist.gam", MergeLiblist (Read (game / "liblist.gam")));
  if (check_only || plan.empty ()) {
    result.files = static_cast<unsigned> (plan.size ());
    return result;
  }
  const auto stamp = std::chrono::high_resolution_clock::now ().time_since_epoch ().count ();
  result.backup = game / "addons" / ("AiBo-backup-4.3.3-" + std::to_string (stamp));
  NoLinks (result.backup);
  fs::create_directories (result.backup);
  std::vector<Change *> ordered;
  for (auto &[key, item] : plan)
    if (key != kIni && key != "liblist.gam")
      ordered.push_back (&item);
  for (const auto *key : { kIni, "liblist.gam" })
    if (plan.count (key))
      ordered.push_back (&plan.at (key));
  // Complete staging/backups before touching any existing game file.
  for (const auto *item : ordered) {
    if (item->existed)
      Write (result.backup / "files" / item->rel, Read (game / item->rel));
    Write (result.backup / "stage" / item->rel, item->bytes);
  }
  std::vector<Change *> committed;
  std::map<std::string, fs::path> renamed;
  try {
    RunningGame ();
    for (auto *item : ordered) {
      if (fail_after >= 0 && static_cast<int> (committed.size ()) == fail_after)
        throw std::runtime_error ("Simulated write failure");
      const auto target = game / item->rel;
      NoLinks (target);
      fs::create_directories (target.parent_path ());
      if (item->existed) {
        auto bak = target;
        bak += ".bak";
        unsigned number = 1;
        while (fs::exists (bak)) {
          bak = target;
          bak += ".bak." + std::to_string (number++);
        }
        NoLinks (bak);
        Replace (target, bak); // Move the old file out before placing the new file.
        renamed[item->rel.generic_string ()] = bak;
      }
      committed.push_back (item); // Includes a rename even if the next write fails.
      Replace (result.backup / "stage" / item->rel, target);
      if (Read (target) != item->bytes)
        throw std::runtime_error ("Verifikasi file terpasang gagal.");
    }
    CheckDll (Read (game / kDll));
    unsigned found = 0;
    for (const auto &line : Lines (Read (game / kIni))) {
      const auto t = Tokens (line);
      if (t.size () >= 2 && (Lower (t[0]) == "win32" || Lower (t[0]) == "linux") && BotModule (t[1])) {
        if (t[0] != "win32" || Slash (t[1]) != kDll)
          throw std::runtime_error ("Entri bot terpasang tidak cocok.");
        ++found;
      }
    }
    if (found != 1)
      throw std::runtime_error ("Entri AiBo.dll terpasang tidak tunggal.");
  } catch (...) {
    bool restored = true;
    for (auto it = committed.rbegin (); it != committed.rend (); ++it) {
      std::error_code ec;
      auto *item = *it;
      const auto dest = game / item->rel;
      try {
        if (item->existed) {
          const auto old = renamed.find (item->rel.generic_string ());
          if (old != renamed.end ()) {
            fs::remove (dest, ec);
            ec.clear ();
            Replace (old->second, dest);
          }
          else {
            const auto tmp = result.backup / "restore" / item->rel;
            Write (tmp, Read (result.backup / "files" / item->rel));
            Replace (tmp, dest);
          }
        }
        else
          fs::remove (dest, ec);
        if (ec)
          restored = false;
      } catch (...) {
        restored = false;
      }
    }
    if (!restored)
      throw std::runtime_error ("Instalasi gagal; sebagian file perlu dipulihkan dari folder AiBo-backup-4.3.3 di addons.");
    throw;
  }
  result.files = static_cast<unsigned> (ordered.size ());
  const auto report =
    "AiBo 4.3.3 installed\r\nDLL: addons/AiBo/bin/AiBo.dll\r\nHUMAN AIM: 100 (Pro profile)\r\nBackup: " + result.backup.filename ().string () +
    "\r\n";
  try {
    Write (result.backup / "INSTALL-RESULT.txt", report);
  } catch (...) { /* Files are committed and verified; a missing optional report cannot undo success. */
  }
  return result;
}
} // namespace aibo_setup
