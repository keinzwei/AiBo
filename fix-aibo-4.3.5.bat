@echo off
setlocal EnableDelayedExpansion
title Fix & Sync AiBo v4.3.5 to GitHub (keinzwei/AiBo)

echo ========================================================
echo   PERBAIKAN & SINKRONISASI PENUH AIBO v4.3.5 KE GITHUB
echo ========================================================
echo.

:: 1. Pastikan Git tersedia di PATH
where git >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files\Git\cmd\git.exe" (
        set "PATH=%PATH%;C:\Program Files\Git\cmd"
    ) else (
        echo [ERROR] Git tidak ditemukan. Pastikan Git for Windows sudah terinstal.
        pause
        exit /b 1
    )
)

:: 2. Pulihkan file v4.3.5 yang terhapus oleh commit b6f60fa (jika source lokal ikut tertimpa 4.3.3)
echo [1/6] Memeriksa riwayat commit dan memulihkan kode v4.3.5...
git fetch origin main
:: Mengambil isi pohon kode v4.3.5 sebelum tertimpa b6f60fa (commit b5bf79b) jika file lokal belum di-update dari source.zip
git show b5bf79b:CHANGELOG.md > CHANGELOG.md.bak 2>nul
if exist CHANGELOG.md.bak (
    findstr /C:"4.3.5" CHANGELOG.md >nul 2>nul
    if !errorlevel! neq 0 (
        echo       Memulihkan kode v4.3.5 dari commit b5bf79b sebelum regresi b6f60fa...
        git checkout b5bf79b -- .
    )
    del /q CHANGELOG.md.bak 2>nul
)

:: 3. Ganti nama file dokumentasi yang masih berlabel 4.3.3 menjadi 4.3.5
echo [2/6] Merapikan nama file dokumentasi 4.3.3 ke 4.3.5...
if exist "HUMAN-AIM-4.3.3-ID.md" git mv -f "HUMAN-AIM-4.3.3-ID.md" "HUMAN-AIM-4.3.5-ID.md" 2>nul || move /y "HUMAN-AIM-4.3.3-ID.md" "HUMAN-AIM-4.3.5-ID.md" >nul
if exist "PRO-PROFILES-4.3.3-ID.md" git mv -f "PRO-PROFILES-4.3.3-ID.md" "PRO-PROFILES-4.3.5-ID.md" 2>nul || move /y "PRO-PROFILES-4.3.3-ID.md" "PRO-PROFILES-4.3.5-ID.md" >nul
if exist "RELEASE-4.3.3-ID.md" git mv -f "RELEASE-4.3.3-ID.md" "RELEASE-4.3.5-ID.md" 2>nul || move /y "RELEASE-4.3.3-ID.md" "RELEASE-4.3.5-ID.md" >nul

:: 4. Update seluruh versi 4.3.3 -> 4.3.5 dan 4,3,3,0 -> 4,3,5,0 di file kunci
echo [3/6] Memperbarui versi di CMakeLists.txt, inc/version.h, inc/human_final.h, dan README...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$files = @('CMakeLists.txt', 'inc/version.h', 'inc/human_final.h', 'README.md', 'README.id.md', 'START-HERE.txt', 'NOTICE-AiBo.md', 'HUMAN-AIM-4.3.5-ID.md', 'PRO-PROFILES-4.3.5-ID.md', 'RELEASE-4.3.5-ID.md');" ^
  "foreach ($f in $files) {" ^
  "  if (Test-Path $f) {" ^
  "    $c = [System.IO.File]::ReadAllText($f);" ^
  "    $c = $c.Replace('4.3.3', '4.3.5').Replace('4,3,3,0', '4,3,5,0').Replace('4, 3, 3', '4, 3, 5');" ^
  "    [System.IO.File]::WriteAllText($f, $c, (New-Object System.Text.UTF8Encoding($false)));" ^
  "    Write-Host '      Updated:' $f" ^
  "  }" ^
  "}"

:: 5. Pastikan #include <cmath> ada di src/graph.cpp agar build MSVC GitHub Actions selalu hijau
echo [4/6] Memastikan header <cmath> pada src/graph.cpp untuk MSVC...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "if (Test-Path 'src/graph.cpp') {" ^
  "  $g = [System.IO.File]::ReadAllText('src/graph.cpp');" ^
  "  if ($g -notmatch '#include\s*<cmath>') {" ^
  "    $g = \"#include <cmath>`r`n\" + $g;" ^
  "    [System.IO.File]::WriteAllText('src/graph.cpp', $g, (New-Object System.Text.UTF8Encoding($false)));" ^
  "    Write-Host '      Added #include <cmath> to src/graph.cpp';" ^
  "  }" ^
  "}"

:: 6. Stage, Commit, Push ke main, dan Perbarui Tag v4.3.5
echo [5/6] Melakukan commit dan push ke branch main...
git add -A
git commit -m "Release AiBo v4.3.5: restore v4.3.5 source tree, bump version headers & docs from 4.3.3 to 4.3.5"
git push origin main

echo [6/6] Memperbarui tag v4.3.5 agar mengarah ke commit terbaru dan memicu CI...
git tag -f v4.3.5
git push origin -f v4.3.5

:: Jika GitHub CLI (gh) terinstal dan file AiBo-4.3.5.zip tersedia, perbaiki nama aset rilis
where gh >nul 2>nul
if %errorlevel% equ 0 (
    if exist "AiBo-4.3.5.zip" (
        echo [OPSIONAL] Mengganti Source.code.zip dengan AiBo-4.3.5.zip di GitHub Releases...
        gh release delete-asset v4.3.5 Source.code.zip -y 2>nul
        gh release upload v4.3.5 "AiBo-4.3.5.zip" --clobber
    )
)

echo.
echo ========================================================
echo   SELESAI! Silakan cek https://github.com/keinzwei/AiBo
echo ========================================================
pause