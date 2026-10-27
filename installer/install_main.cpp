// AiBo 4.3.3 native installer worker. GPL-2.0-or-later.
#include "install_core.h"
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <vector>
#ifdef _WIN32
  #ifndef NOMINMAX
#define NOMINMAX
#endif
  #include <windows.h>
  #include <shellapi.h>
#endif
namespace fs = std::filesystem;
int Run (const std::vector<fs::path> &args) {
  fs::path report;
  if (args.size () == 5)
    report = args[4];
  auto say = [&] (const std::string &text) {
    if (!report.empty ()) {
      std::ofstream f (report, std::ios::binary);
      f << text << "\r\n";
    }
#ifndef _WIN32
    std::cout << text << '\n';
#endif
  };
  try {
    if (args.size () != 5 || (args[1] != "--install" && args[1] != "--check"))
      throw std::runtime_error ("Argumen installer tidak valid.");
    int fail_after = -1;
#ifdef AIBO_INSTALL_TESTS
    if (const char *value = std::getenv ("AIBO_TEST_FAIL_AFTER"))
      fail_after = std::atoi (value);
#endif
    const auto r = aibo_setup::Install (args[2], args[3], args[1] == "--check", fail_after);
    const auto u8 = r.backup.u8string ();
    say ("AiBo 4.3.3 berhasil. DLL: addons/AiBo/bin/AiBo.dll; HUMAN AIM 100. Files=" + std::to_string (r.files) + "; preserved=" +
         std::to_string (r.preserved) + "; migrated=" + std::to_string (r.migrated) + "; backup=" + std::string (u8.begin (), u8.end ()));
    return 0;
  } catch (const std::exception &e) {
    say (std::string ("Pemasangan AiBo dibatalkan: ") + e.what ());
    return 1;
  }
}
#ifdef _WIN32
int WINAPI wWinMain (HINSTANCE, HINSTANCE, PWSTR, int) {
  int count = 0;
  auto argv = CommandLineToArgvW (GetCommandLineW (), &count);
  if (!argv)
    return 1;
  std::vector<fs::path> args;
  for (int i = 0; i < count; ++i)
    args.emplace_back (argv[i]);
  LocalFree (argv);
  return Run (args);
}
#else
int main (int argc, char **argv) {
  std::vector<fs::path> args;
  for (int i = 0; i < argc; ++i)
    args.emplace_back (argv[i]);
  return Run (args);
}
#endif
