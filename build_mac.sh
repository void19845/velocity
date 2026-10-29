#!/bin/sh
# Compile VELOCITY pour macOS (binaire universel arm64 + x86_64) et assemble Velocity.app + Velocity-macOS.zip.
# Necessite les Command Line Tools d'Xcode (xcode-select --install). A lancer sur un Mac : ./build_mac.sh
set -e
cd "$(dirname "$0")"

APP=Velocity.app
rm -rf "$APP" build
mkdir -p build "$APP/Contents/MacOS" "$APP/Contents/Resources"

# VELOCITY_BUILD : numero de build donne par la CI de release (bouton MISE A JOUR) ; absent = build 0 (dev)
clang -O2 -std=gnu11 -fobjc-arc -Wall -Wno-deprecated-declarations ${VELOCITY_BUILD:+-DVELOCITY_BUILD=$VELOCITY_BUILD} \
    -mmacosx-version-min=10.15 -arch arm64 -arch x86_64 \
    main.c mac_platform.m \
    -framework Cocoa -framework OpenGL -framework AudioToolbox -framework CoreText -framework Carbon \
    -o "$APP/Contents/MacOS/velocity"

cat > "$APP/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>VELOCITY</string>
    <key>CFBundleDisplayName</key><string>VELOCITY</string>
    <key>CFBundleIdentifier</key><string>com.velocity.game</string>
    <key>CFBundleExecutable</key><string>velocity</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>CFBundleVersion</key><string>1</string>
    <key>LSMinimumSystemVersion</key><string>10.15</string>
    <key>NSHighResolutionCapable</key><false/>
    <key>LSApplicationCategoryType</key><string>public.app-category.games</string>
</dict>
</plist>
EOF

# signature ad hoc (obligatoire sur Apple Silicon ; ne remplace pas une vraie signature pour la distribution)
codesign --force --deep -s - "$APP"

ditto -c -k --keepParent "$APP" Velocity-macOS.zip
rm -rf build
echo "OK : $APP et Velocity-macOS.zip"
