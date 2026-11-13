#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"
echo "=== Publishing AiBo 4.3.5 to https://github.com/keinzwei/AiBo/ ==="
if [ ! -d .git ]; then
  git init
  git branch -M main
  git remote add origin https://github.com/keinzwei/AiBo.git
else
  git remote set-url origin https://github.com/keinzwei/AiBo.git || git remote add origin https://github.com/keinzwei/AiBo.git
fi
git add -A
git commit -m "Release AiBo 4.3.5: 50/50 AWP-Rifle, Pro Deagle Quick-Switch & Travel, Free CT Routes, Min 16 Initial Bots" || true
git tag -f v4.3.5
git push -u origin main --force
git push origin v4.3.5 --force
if command -v gh >/dev/null 2>&1; then
  gh release create v4.3.5 "AiBo-4.3.5-Setup.exe" --title "AiBo 4.3.5" --notes-file CHANGELOG.md || gh release upload v4.3.5 "AiBo-4.3.5-Setup.exe" --clobber
fi
echo "[DONE] Published AiBo 4.3.5 to https://github.com/keinzwei/AiBo/"
