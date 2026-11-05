#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
SDL=/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64/libSDL2-2.0.0.dylib
[ -f "$SDL" ] || { echo "CrossOver SDL2 bulunamadı" >&2; exit 1; }
[ -f /opt/homebrew/include/SDL2/SDL.h ] || { echo "SDL2 başlıkları bulunamadı" >&2; exit 1; }
mkdir -p build
clang -arch x86_64 -dynamiclib -O2 -Wall -Wextra -I /opt/homebrew/include/SDL2 sdl_bridge.c \
  -Wl,-reexport_library,"$SDL" -Wl,-rpath,/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64 -install_name @rpath/libwhe-bridge.dylib \
  -o build/libSDL2-2.0.0.dylib
echo "Deneme köprüsü derlendi."
