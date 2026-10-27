// AiBo 4.3.3 installer engine. GPL-2.0-or-later.
#pragma once
#include <filesystem>
#include <string>
namespace aibo_setup {
struct Result {
  std::filesystem::path game, backup;
  unsigned files {}, preserved {}, migrated {};
};
std::string MergePlugins (const std::string &text);
std::string MergeLiblist (const std::string &text);
Result Install (const std::filesystem::path &payload, const std::filesystem::path &target, bool check_only = false, int fail_after = -1);
}
