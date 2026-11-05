#!/bin/bash
set -euo pipefail
RUNTIME="$HOME/Library/Application Support/Wireless HE Studio/CrossOver Runtime/CrossOver.app/Contents/SharedSupport/CrossOver"
PREFIX="$HOME/Library/Application Support/CrossOver/Bottles/Steam"
if [[ ! -x "$RUNTIME/bin/cxstart" || ! -d "$PREFIX" ]]; then
  echo 'Test CrossOver kopyası veya Steam şişesi bulunamadı.'
  read -r -p 'Kapatmak için Enter: ' _
  exit 1
fi
# Never interrupt an existing bottle; the user closes it first.
for pid in $(pgrep -f '/bin/wineserver' || true); do
  if lsof -p "$pid" -Fn 2>/dev/null | grep -Fxq "n$PREFIX"; then
    if lsof -nP -iUDP:27861 2>/dev/null | grep -q winedevic; then
      echo 'Köprü zaten açık; Steam mevcut test oturumunda başlatılıyor.'
      exec "$RUNTIME/bin/cxstart" --bottle Steam --no-gui --no-wait 'C:\Program Files (x86)\Steam\steam.exe'
    fi
    echo 'Steam şişesi köprü olmadan açık. CrossOver içindeki Steam’i kapatıp tekrar dene.'
    read -r -p 'Kapatmak için Enter: ' _
    exit 1
  fi
done
"$RUNTIME/bin/cxstart" --bottle Steam --no-gui --wait reg add 'HKLM\System\CurrentControlSet\Services\winebus\Parameters' /v 'Enable SDL' /t REG_DWORD /d 1 /f
# The only server just started for this closed bottle was created by the command above.
WINEPREFIX="$PREFIX" "$RUNTIME/bin/wineserver" -k
exec "$RUNTIME/bin/cxstart" --bottle Steam --no-gui --no-wait 'C:\Program Files (x86)\Steam\steam.exe'
