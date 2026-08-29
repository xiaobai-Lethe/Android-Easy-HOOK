#!/bin/bash
#
# 2026年08月29日
# 生成 flyme-fix-vibrator 的 Java 回调类 dex，并转成可内嵌进 so 的 C 头文件。
#
# 与 build_art_demo_dex.sh 同理：回调类只依赖 java.lang 与 java.lang.reflect，
# 普通 JDK 的 javac 即可编译，不需要 android.jar；.class 到 .dex 由 tools/r8.jar(内含 D8)完成。
# 改动回调逻辑(VibratorRerouteHooker.java)后必须重新运行本脚本。
# 2026 BY white

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"

SRC="$ROOT/plugins/flyme-fix-vibrator/java/org/easyhook/flymevib/VibratorRerouteHooker.java"
R8="$SCRIPT_DIR/r8.jar"
BUILD="$ROOT/build-flyme-vibrator-dex"
OUT_HEADER="$ROOT/plugins/flyme-fix-vibrator/vibrator_reroute_hooker_dex.h"

for path in "$SRC" "$R8"; do
    if [ ! -f "$path" ]; then
        echo "missing required file: $path" >&2
        exit 1
    fi
done

rm -rf "$BUILD"
mkdir -p "$BUILD/classes"

echo "=== javac ==="
javac -encoding UTF-8 --release 8 -d "$BUILD/classes" "$SRC"

echo "=== d8 ==="
java -cp "$R8" com.android.tools.r8.D8 --min-api 30 --release \
    --output "$BUILD" "$BUILD/classes/org/easyhook/flymevib/VibratorRerouteHooker.class"

DEX="$BUILD/classes.dex"
echo "classes.dex size=$(stat -c '%s' "$DEX") bytes"

echo "=== emit header ==="
{
    echo '#pragma once'
    echo ''
    echo '#include <cstddef>'
    echo ''
    echo '// VibratorRerouteHooker 的 dex 字节，由 tools/build_flyme_vibrator_dex.sh 生成，请勿手改。'
    echo '// 改回调逻辑请改 java 源后重新运行脚本。'
    echo ''
    echo 'namespace flyme_vibrator {'
    echo ''
    echo 'inline constexpr unsigned char kVibratorRerouteHookerDex[] = {'
    od -An -v -tx1 "$DEX" | awk '{ for (i = 1; i <= NF; i++) printf "0x%s, ", $i }' \
        | fold -w 96 | sed 's/^/    /'
    echo ''
    echo '};'
    echo ''
    echo 'inline constexpr std::size_t kVibratorRerouteHookerDexSize = sizeof(kVibratorRerouteHookerDex);'
    echo ''
    echo '}  // namespace flyme_vibrator'
} > "$OUT_HEADER"

rm -rf "$BUILD"
echo "header -> $OUT_HEADER"
