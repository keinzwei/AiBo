# Build AiBo 4.3.3

Source sudah mencakup dependensi vendored. Kebutuhan: CMake 3.22+, compiler C++20,
Python 3, dan Ninja/Make. Build DLL tidak membutuhkan runtime AMXX/Metamod.
Jalankan dari root source/repository.

## Uji native

```sh
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug -DAIBO_NATIVE=ON -DAIBO_64BIT=ON -DAIBO_BUILD_TESTS=ON -DAIBO_WITH_TLS=OFF -DAIBO_WITH_AMXX=OFF -DAIBO_LTO=OFF
cmake --build build/test --parallel 4
ctest --test-dir build/test --output-on-failure --timeout 90 --parallel 2
```

Fixture numerik sintetis 1200 input menguji pitch/yaw/fire pada level nol.
Tidak ada source versi lama dalam repository. Sudut memakai toleransi 0.001 derajat
untuk perbedaan floating point compiler; keputusan menembak harus sama persis.
Tes host bukan pertandingan game sebenarnya.

## Windows x86 DLL dengan MinGW

```sh
cmake -S . -B build/windows -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x86-mingw.cmake -DCMAKE_BUILD_TYPE=Release -DAIBO_WITH_TLS=ON -DAIBO_WITH_AMXX=OFF -DAIBO_LTO=OFF
cmake --build build/windows --parallel 4
python3 tools/check_dll.py --arch i386 build/windows/AiBo.dll
```

Butuh i686 MinGW-w64 POSIX GCC/G++, binutils dan windres di PATH. Toolchain yang
diekstrak manual mungkin membutuhkan CMAKE_RC_FLAGS dengan
`--preprocessor=/path/i686-w64-mingw32-gcc-posix --preprocessor-arg=-E --preprocessor-arg=-xc --preprocessor-arg=-DRC_INVOKED`.
Versi resource DLL 4.3.3.1; label produk AiBo 4.3.3.

MSVC (resep disediakan; belum dijalankan):

```powershell
cmake -S . -B build/windows -A Win32 -DAIBO_WITH_TLS=ON -DAIBO_WITH_AMXX=OFF -DAIBO_LTO=OFF
cmake --build build/windows --config Release --parallel 4
python tools/check_dll.py --arch i386 build/windows/Release/AiBo.dll
```

Workflow .github/workflows/build.yml menjalankan regresi Linux serta build/ABI DLL
Windows. Workflow ini diperbarui untuk struktur source saat ini; belum dijalankan
melalui GitHub Actions. Tidak ada riwayat git dari proyek asal dalam ZIP.

## Installer EXE

Dari paket lengkap, folder `../install/cstrike` menyediakan komponen runtime asli
yang ditambahkan pengguna. Source GitHub hanya menyertakan kode/config, bukan DLL
Metamod/AMXX hasil kompilasi. Siapkan runtime terpisah untuk membangun installer.
Worker C++20 dapat dibangun pada Linux sebagai berikut:

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -DAIBO_INSTALL_TESTS installer/install_core.cpp installer/install_main.cpp -o build/AiBoInstall-test
i686-w64-mingw32-g++-posix -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -static -static-libstdc++ -static-libgcc -mwindows -municode installer/install_core.cpp installer/install_main.cpp -lshell32 -o build/AiBoInstall.exe
python3 tools/build_installer.py --binary build/windows/AiBo.dll --worker build/AiBoInstall.exe --payload-base ../install/cstrike --output ../AiBo-4.3.3-Setup.exe
python3 tests/installer_test.py build/AiBoInstall-test build/installer/payload
python3 tools/package_aibo.py --binary build/windows/AiBo.dll --installer ../AiBo-4.3.3-Setup.exe --output ../AiBo-4.3.3-Windows.zip
```

NSIS 3 harus tersedia (`--makensis` dapat menentukan executable). Builder memeriksa
plugins.ini, DLL PE32, default Pro serta hash kedua file unggahan sebelum makensis.
Source payload pengguna tidak diambil dari internet. Untuk source tanpa paket
lengkap, ubah `--payload-base` ke folder runtime cstrike yang disiapkan.

File LICENSE/COPYING.GPL-2.0 berlaku untuk fork gabungan; atribusi komponen dan graph
ada di NOTICE-AiBo.md dan cfg/addons/AiBo/licenses/. Hasil build, log dan ZIP dikecualikan
oleh .gitignore. Untuk GitHub, unggah isi folder source/ sebagai root repository.
