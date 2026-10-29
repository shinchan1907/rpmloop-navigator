#!/bin/bash
set -e

SDK="/opt/android-sdk"
BUILD_TOOLS="$SDK/build-tools/34.0.0"
PLATFORM="$SDK/platforms/android-34/android.jar"
JAVAC="/usr/lib/jvm/java-17-openjdk-amd64/bin/javac"
DIR="/mnt/e/BlenderForge/rpmlopp-nav/android"

cd "$DIR"
echo "=== Step 1: Cleaning and creating build directories ==="
rm -rf build
mkdir -p build/gen build/obj build/apk

echo "=== Step 2: Compiling Android Resources with AAPT2 ==="
$BUILD_TOOLS/aapt2 compile --dir res -o build/compiled_res.zip

echo "=== Step 3: Linking Manifest & Assets with AAPT2 ==="
$BUILD_TOOLS/aapt2 link -I "$PLATFORM" \
    --manifest AndroidManifest.xml \
    -o build/apk/app-unaligned.apk \
    -A assets \
    --java build/gen \
    build/compiled_res.zip

echo "=== Step 4: Compiling Java Sources ==="
$JAVAC -d build/obj \
    -cp "$PLATFORM" \
    -sourcepath "src:build/gen" \
    src/com/sunnygupta/rpmloopnav/*.java \
    build/gen/com/sunnygupta/rpmloopnav/R.java \
    -target 8 -source 8

echo "=== Step 5: Converting to Dalvik Executable (DEX) with D8 ==="
$BUILD_TOOLS/d8 --lib "$PLATFORM" --output build/apk/classes.zip build/obj/com/sunnygupta/rpmloopnav/*.class
unzip -q build/apk/classes.zip -d build/apk/
rm -f build/apk/classes.zip

echo "=== Step 6: Packaging DEX into APK ==="
cd build/apk
zip -u -0 app-unaligned.apk classes.dex
cd "$DIR"

echo "=== Step 7: ZipAligning APK ==="
$BUILD_TOOLS/zipalign -f -p 4 build/apk/app-unaligned.apk build/RPMLoopNav-unaligned.apk

echo "=== Step 8: Generating Debug Signing Keystore ==="
if [ ! -f build/debug.keystore ]; then
    keytool -genkeypair -v \
        -keystore build/debug.keystore \
        -storepass android \
        -alias androiddebugkey \
        -keypass android \
        -keyalg RSA \
        -keysize 2048 \
        -validity 10000 \
        -dname "CN=Android Debug,O=Android,C=US"
fi

echo "=== Step 9: Signing APK with APKSIGNER ==="
$BUILD_TOOLS/apksigner sign \
    --ks build/debug.keystore \
    --ks-pass pass:android \
    --ks-key-alias androiddebugkey \
    --key-pass pass:android \
    --out RPMLoopNav.apk \
    build/RPMLoopNav-unaligned.apk

echo "=== Step 10: Verifying Signed APK ==="
$BUILD_TOOLS/apksigner verify RPMLoopNav.apk

# Copy APK to dashboard/public so user can download directly from phone browser
mkdir -p "$DIR/../dashboard/public"
cp RPMLoopNav.apk "$DIR/../dashboard/public/RPMLoopNav.apk"

echo "=========================================================="
echo " BUILD SUCCESSFUL! "
echo " APK created at: $DIR/RPMLoopNav.apk"
echo " Downloadable at: http://<PC_IP>:8080/RPMLoopNav.apk"
echo "=========================================================="
