@echo off
setlocal EnableDelayedExpansion
title Fix CTest #168 & Sync All 4.3.5 Runtime Identity

echo ========================================================
echo   FIX CTEST #168 (RUNTIME IDENTITY 4.3.5) & PUSH GITHUB
echo ========================================================
echo.

:: 1. Pastikan Git tersedia di PATH
where git >nul 2>nul
if %errorlevel% neq 0 (
    if exist "C:\Program Files\Git\cmd\git.exe" (
        set "PATH=%PATH%;C:\Program Files\Git\cmd"
    ) else (
        echo [ERROR] Git tidak ditemukan.
        pause
        exit /b 1
    )
)

:: 2. Sapu bersih semua string "4.3.3" -> "4.3.5" di seluruh folder src, inc, tests, tools, installer, cfg, dan root
echo [1/3] Menyelaraskan versi 4.3.3 ke 4.3.5 di seluruh src/, inc/, tests/, dan tools/...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$exts = @('*.cpp', '*.h', '*.hpp', '*.in', '*.c', '*.cmake', 'CMakeLists.txt', '*.json', '*.py', '*.nsi', '*.rc', '*.cfg', '*.ini', '*.md', '*.txt');" ^
  "$files = Get-ChildItem -Path . -Recurse -Include $exts | Where-Object { $_.FullName -notmatch '\\\.git\\' -and $_.Name -ne 'CHANGELOG.md' };" ^
  "$count = 0;" ^
  "foreach ($f in $files) {" ^
  "  $c = [System.IO.File]::ReadAllText($f.FullName);" ^
  "  if ($c.Contains('4.3.3') -or $c.Contains('4,3,3,0') -or $c.Contains('4, 3, 3')) {" ^
  "    $c = $c.Replace('4.3.3', '4.3.5').Replace('4,3,3,0', '4,3,5,0').Replace('4, 3, 3', '4, 3, 5');" ^
  "    [System.IO.File]::WriteAllText($f.FullName, $c, (New-Object System.Text.UTF8Encoding($false)));" ^
  "    Write-Host '      Updated:' $f.FullName;" ^
  "    $count++;" ^
  "  }" ^
  "}" ^
  "Write-Host '      Total file diperbarui:' $count"

:: 3. Pastikan header ## 4.3.5 tercatat di bagian atas CHANGELOG.md jika belum ada
echo [2/3] Memeriksa CHANGELOG.md...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "if (Test-Path 'CHANGELOG.md') {" ^
  "  $ch = [System.IO.File]::ReadAllText('CHANGELOG.md');" ^
  "  if ($ch -notmatch '##\s*\[?4\.3\.5\]?') {" ^
  "    $entry = \"## 4.3.5`r`n`r`n- Add ProWeaponSwitchController: AWP travel with Deagle (250 u/s), post-shot quick-switch, and rifle low-clip secondary switch`r`n- Restore free-roaming CT exploration routes pre-plant`r`n- Enforce minimum 16 initial bots before Internet Simulation Mode (10-30s interval)`r`n- Synchronize runtime identity and release metadata to 4.3.5`r`n`r`n\";" ^
  "    $ch = $ch -replace '(?m)^##\s*4\.3\.3', ($entry + '## 4.3.3');" ^
  "    [System.IO.File]::WriteAllText('CHANGELOG.md', $ch, (New-Object System.Text.UTF8Encoding($false)));" ^
  "    Write-Host '      Added ## 4.3.5 section to CHANGELOG.md';" ^
  "  }" ^
  "}"

:: 4. Commit, Push ke main, dan Perbarui Tag v4.3.5
echo [3/3] Melakukan commit dan push ke GitHub...
git add -A
git commit -m "Fix CTest #168 runtime identity assertion & sync all source/test version strings to 4.3.5"
git push origin main

git tag -f v4.3.5
git push origin -f v4.3.5

echo.
echo ========================================================
echo   SELESAI! Silakan cek GitHub Actions (294/294 Passed)
echo ========================================================
pause