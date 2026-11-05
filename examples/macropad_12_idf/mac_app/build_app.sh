#!/bin/bash
set -euo pipefail
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP_DIR="$PROJECT_DIR/mac_app/Wireless HE Studio.app"
PYTHON_BIN="$HOME/.espressif/python_env/idf6.1_py3.13_env/bin/python"
[ -x "$PYTHON_BIN" ] || { echo "ESP-IDF Python ortamı bulunamadı: $PYTHON_BIN" >&2; exit 1; }
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS" "$APP_DIR/Contents/Resources"
cp "$PROJECT_DIR/mac_app/assets/macropad.usdz" "$APP_DIR/Contents/Resources/macropad.usdz"
ICON_WORK="$(mktemp -d)"
trap 'rm -rf "$ICON_WORK"' EXIT
mkdir -p "$APP_DIR/Contents/Resources/en.lproj"
cp "$PROJECT_DIR/mac_app/assets/en.lproj/Localizable.strings" "$APP_DIR/Contents/Resources/en.lproj/Localizable.strings"
cp "$PROJECT_DIR/mac_app/assets/studio-grid.svg" "$APP_DIR/Contents/Resources/studio-grid.svg"
xcrun actool --compile "$APP_DIR/Contents/Resources" --platform macosx --minimum-deployment-target 26.0 --target-device mac --app-icon WirelessHEStudio --output-partial-info-plist "$ICON_WORK/icon-info.plist" "$PROJECT_DIR/mac_app/assets/WirelessHEStudio.icon" >/dev/null
swiftc -O "$PROJECT_DIR/mac_app/Native.swift" "$PROJECT_DIR/mac_app/Model3D.swift" "$PROJECT_DIR/mac_app/BLEStudio.swift" -o "$APP_DIR/Contents/MacOS/WirelessHEStudio" -framework Cocoa -framework SwiftUI -framework Charts -framework SceneKit
python3 - "$APP_DIR/Contents/Info.plist" "$PROJECT_DIR" "$PYTHON_BIN" <<'PY'
import plistlib,sys
with open(sys.argv[1],'wb') as f:
 plistlib.dump({'CFBundleDevelopmentRegion':'tr','CFBundleLocalizations':['tr','en'],'CFBundleName':'Wireless HE Studio','CFBundleDisplayName':'Wireless HE Studio','CFBundleIdentifier':'local.wirelesshe.studio','CFBundleExecutable':'WirelessHEStudio','CFBundleIconFile':'WirelessHEStudio','CFBundleIconName':'WirelessHEStudio','CFBundlePackageType':'APPL','CFBundleShortVersionString':'0.1.7','CFBundleSupportedPlatforms':['MacOSX'],'LSMinimumSystemVersion':'26.0','NSHighResolutionCapable':True,'NSBluetoothAlwaysUsageDescription':'Connect to Wireless HE to configure lighting and key assignments.','TestProjectPath':sys.argv[2],'TestPythonPath':sys.argv[3],'NSAppTransportSecurity':{'NSAllowsLocalNetworking':True}},f)
PY
touch "$APP_DIR"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$APP_DIR" || true
printf 'Hazır: %s\n' "$APP_DIR"
