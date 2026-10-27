# Contributing to AiBo

Please open AiBo-specific issues and pull requests in this repository.

## Report a problem

Include the AiBo version, operating system, engine/GameDLL, Metamod and AMX Mod X versions, map, plugins, relevant configuration and reproduction steps. A short video and console output help with navigation/aim problems. Remove passwords and personal information from logs before posting. For custom graphs, identify their source and license.

## Make a change

Keep patches focused and preserve affected components' attribution/license notices. Describe the triggering case, changed behavior and validation. Keep generated binaries, caches and personal learning files out of source commits.

Follow the [build guide](docs/human-copycat/BUILD.md) and run relevant native tests. DLL boundary changes also need a Windows x86 build and `tools/check_dll.py`. Installer changes should run `tests/installer_test.py` against a staged payload. Distinguish fake-engine tests from actual Windows/game testing.

Persistent data formats and native graph compatibility need compatibility handling and meaningful regression coverage when changed. Contributions should remain compatible with the applicable component/fork licenses. There is no promise about response times, release dates or acceptance.
