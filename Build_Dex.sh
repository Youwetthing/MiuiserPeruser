#!/usr/bin/env bash
# Build_Dex.sh -- compile ShizukuDirectLoader.java -> rish_shizuku_direct.dex
#
# Required env (any one config works):
#   ANDROID_HOME or ANDROID_SDK_ROOT    -> path to Android SDK root
#   D8                                  -> path to d8 binary  (overrides search)
#   ANDROID_JAR                         -> path to android.jar (overrides search)
#
# Optional:
#   API_LEVEL          minSdkVersion passed to d8         (default 26)
#   PROVIDER_JAR       path to shizuku-provider.jar       (optional but recommended)
#
# Output:
#   $REPO/rish_shizuku_direct.dex
#
# Notes:
#   * Android 14+ refuses to load writable DEX via app_process; we chmod 400 at
#     the end so the file is ready to drop next to the existing rish_shizuku.dex.
#   * shizuku-api.jar is pulled from java-helper/libs unconditionally.
#   * If PROVIDER_JAR is set or shizuku-provider.jar is in libs/, it goes on the
#     classpath too -- enabling the "Strategy A" attach path in the loader.

set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
SRC="$REPO/java-helper/src/main/java/ShizukuDirectLoader.java"
LIBS="$REPO/java-helper/libs"
OUT_CLASSES="$REPO/build/dex-classes"
OUT_DEX="$REPO/rish_shizuku_direct.dex"
API_LEVEL="${API_LEVEL:-26}"

[ -f "$SRC" ] || { echo "[!] source not found: $SRC" >&2; exit 1; }

# ---- locate android.jar ----
if [ -z "${ANDROID_JAR:-}" ]; then
  SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
  if [ -z "$SDK" ]; then
    echo "[!] set ANDROID_HOME (or ANDROID_JAR) so we can find android.jar" >&2
    exit 1
  fi
  # pick the highest platform we can find
  ANDROID_JAR="$(ls -1 "$SDK"/platforms/android-*/android.jar 2>/dev/null | sort -V | tail -n1)"
  if [ -z "$ANDROID_JAR" ] || [ ! -f "$ANDROID_JAR" ]; then
    echo "[!] no android.jar under $SDK/platforms/android-*/; set ANDROID_JAR" >&2
    exit 1
  fi
fi
echo "[*] android.jar : $ANDROID_JAR"

# ---- locate d8 ----
if [ -z "${D8:-}" ]; then
  SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
  if [ -n "$SDK" ]; then
    D8="$(ls -1 "$SDK"/build-tools/*/d8 2>/dev/null | sort -V | tail -n1 || true)"
  fi
  if [ -z "${D8:-}" ]; then
    D8="$(command -v d8 || true)"
  fi
fi
[ -n "${D8:-}" ] && [ -x "$D8" ] || { echo "[!] d8 not found; set D8 or install build-tools" >&2; exit 1; }
echo "[*] d8         : $D8"

# ---- classpath ----
CP="$ANDROID_JAR"
for j in "$LIBS"/shizuku-api.jar "$LIBS"/shizuku-provider.jar "${PROVIDER_JAR:-}"; do
  [ -n "$j" ] && [ -f "$j" ] && CP="$CP:$j"
done
echo "[*] classpath  : $CP"

# ---- javac ----
rm -rf "$OUT_CLASSES"
mkdir -p "$OUT_CLASSES"
echo "[*] javac      : compiling..."
javac -source 1.8 -target 1.8 \
      -bootclasspath "$ANDROID_JAR" \
      -classpath "$CP" \
      -d "$OUT_CLASSES" \
      "$SRC"

# ---- d8 ----
echo "[*] d8         : dexing..."
INPUT_JARS=()
INPUT_JARS+=("$LIBS/shizuku-api.jar")
[ -f "$LIBS/shizuku-provider.jar" ] && INPUT_JARS+=("$LIBS/shizuku-provider.jar")
[ -n "${PROVIDER_JAR:-}" ] && [ -f "${PROVIDER_JAR}" ] && INPUT_JARS+=("${PROVIDER_JAR}")

# d8 wants classes + jars together; --output writes a directory containing classes.dex.
TMP_OUT="$(mktemp -d)"
"$D8" --min-api "$API_LEVEL" \
      --lib "$ANDROID_JAR" \
      --output "$TMP_OUT" \
      "${INPUT_JARS[@]}" \
      $(find "$OUT_CLASSES" -name '*.class')

mv "$TMP_OUT/classes.dex" "$OUT_DEX"
rm -rf "$TMP_OUT"

# Android 14+ needs the dex to be non-writable to be loaded via app_process.
chmod 400 "$OUT_DEX"

echo "[+] wrote $OUT_DEX ($(stat -c%s "$OUT_DEX") bytes)"
echo ""
echo "Smoke-test on device:"
echo "  RISH_APPLICATION_ID=com.termux /system/bin/app_process \\"
echo "      -Djava.class.path=$OUT_DEX \\"
echo "      \$HOME --nice-name=miuiser_shz_helper \\"
echo "      ShizukuDirectLoader --uds /data/data/com.termux/files/usr/tmp/miuiser-shizuku.sock"
