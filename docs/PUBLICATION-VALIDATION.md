# Publication validation — 2026-10-27

This report records checks rerun while preparing the supplied AiBo 4.3.3 package for GitHub publication. Two zero-valued navigation priority-queue seeds were made explicitly float (`0.0f`) for strict MSVC compilation; their values and intended behavior are unchanged. Installer implementation and shipped binaries were not changed. Repository-facing documentation and issue templates were prepared for the independent fork.

| Check | Result |
| --- | --- |
| Native Debug build, GCC 13.3, C++20 | Passed, including bot library and test targets. |
| CTest native fake-engine regression suite | **293/293 passed**, no failures. |
| Native installer worker, strict GCC warnings | Compiled successfully. |
| Installer integration scenarios against supplied runtime payload | **11/11 passed**. |
| Supplied Windows DLL | PE i386, ten required exports, system-only imports accepted by `tools/check_dll.py`. |
| Supplied installer/DLL SHA-256 | Both match the original `BUILD_INFO.json`. |
| New documentation's relative links | Checked against repository files. |

## Reproduction

```sh
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DAIBO_NATIVE=ON -DAIBO_64BIT=ON -DAIBO_BUILD_TESTS=ON \
  -DAIBO_WITH_TLS=OFF -DAIBO_WITH_AMXX=OFF -DAIBO_LTO=OFF
cmake --build build/test --parallel 4
ctest --test-dir build/test --output-on-failure --timeout 90 --parallel 2

g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror \
  -DAIBO_INSTALL_TESTS installer/install_core.cpp installer/install_main.cpp \
  -o build/AiBoInstall-test
python3 tests/installer_test.py build/AiBoInstall-test /path/to/full-package/install/cstrike
python3 tools/check_dll.py --arch i386 /path/to/full-package/install/cstrike/addons/AiBo/bin/AiBo.dll
```

## Original binary checksums

```text
01774a217e4789b0a816c9e74082d4d755569ee6eb5f95fece90a8ef6a5781ca  AiBo-4.3.3-Setup.exe
aae08c70422d6450acca71df7140fb9e19fd06c12b8a09c950951f435df42ab5  AiBo.dll
```

The shipped Windows binaries were verified, not rebuilt or re-signed during this publication step. Tests ran on Linux and did not execute CS 1.6, Windows installer UI or Windows process-lock behavior. GitHub Actions status must be assessed from actual workflow runs. See the original [validation record](VALIDATION.md) for additional scope and limits.
