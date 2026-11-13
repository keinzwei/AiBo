@echo off
setlocal
cd /d "%~dp0"
echo ============================================================
echo   AiBo 4.3.5 - GitHub Repository & Release Publisher
echo   Target: https://github.com/keinzwei/AiBo/
echo ============================================================

where git >nul 2>nul
if errorlevel 1 (
  echo [ERROR] Git belum terpasang di PATH. Silakan pasang Git for Windows terlebih dahulu.
  pause
  exit /b 1
)

if not exist ".git" (
  git init
  git branch -M main
  git remote add origin https://github.com/keinzwei/AiBo.git
) else (
  git remote set-url origin https://github.com/keinzwei/AiBo.git 2>nul || git remote add origin https://github.com/keinzwei/AiBo.git
)

git add -A
git commit -m "Release AiBo 4.3.5: 50/50 AWP-Rifle, Pro Deagle Quick-Switch & Travel, Free CT Routes, Min 16 Initial Bots"
git tag -f v4.3.5
git push -u origin main --force
git push origin v4.3.5 --force

where gh >nul 2>nul
if not errorlevel 1 (
  gh release view v4.3.5 >nul 2>nul
  if errorlevel 1 (
    gh release create v4.3.5 "AiBo-4.3.5-Setup.exe" --title "AiBo 4.3.5" --notes-file CHANGELOG.md
  ) else (
    gh release upload v4.3.5 "AiBo-4.3.5-Setup.exe" --clobber
  )
)

echo.
echo [SELESAI] Source code dan tag v4.3.5 telah dikirim ke https://github.com/keinzwei/AiBo/
pause
