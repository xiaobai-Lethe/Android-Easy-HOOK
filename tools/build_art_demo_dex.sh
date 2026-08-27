#!/bin/bash
#
# 2026年08月28日01时30分
# 生成 ART hook 演示所需的 Java 回调类 dex，并转成可内嵌进 so 的 C 头文件。
#
# 注入型框架无法指望目标进程里存在自己的 Java 类，而 LSPlant 的方法级 hook
# 必须要有一个 Java 回调对象，因此回调类要随 so 一同分发：
# 这里把它编译成 dex 后转为字节数组，运行时再从内存加载进目标进程。
#
# 回调类只依赖 java.lang 与 java.lang.reflect，普通 JDK 的 javac 即可编译，
# 构建链上不需要 android.jar；.class 到 .dex 由随仓库放置的 r8.jar(内含 D8)完成。
#
# 改动回调逻辑后必须重新运行本脚本，否则内嵌的仍是旧 dex。
# 2026 BY white

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"

SRC="$ROOT/plugins/art_demo/java/org/easyhook/demo/StringReplaceHooker.java"
R8="$SCRIPT_DIR/r8.jar"
BUILD="$ROOT/build-art-demo-dex"
OUT_HEADER="$ROOT/plugins/art_demo/string_replace_hooker_dex.h"

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
    --output "$BUILD" "$BUILD/classes/org/easyhook/demo/StringReplaceHooker.class"

DEX="$BUILD/classes.dex"
echo "classes.dex size=$(stat -c '%s' "$DEX") bytes"

echo "=== emit header ==="
{
    echo '#pragma once'
    echo ''
    echo '#include <cstddef>'
    echo ''
    echo '/**'
    echo " * $(date '+%Y年%m月%d日%H时%M分')"
    echo ' * StringReplaceHooker 的 dex 字节，由 tools/build_art_demo_dex.sh 从对应 Java 源自动生成。'
    echo ' * 请勿编辑：修改回调逻辑应修改 java 源后重新运行该脚本。'
    echo ' * 2026 BY white'
    echo ' */'
    echo ''
    echo 'namespace easy_user_hooks::art_demo {'
    echo ''
    echo 'inline constexpr unsigned char kStringReplaceHookerDex[] = {'
    od -An -v -tx1 "$DEX" | awk '{ for (i = 1; i <= NF; i++) printf "0x%s, ", $i }' \
        | fold -w 96 | sed 's/^/    /'
    echo ''
    echo '};'
    echo ''
    echo 'inline constexpr std::size_t kStringReplaceHookerDexSize = sizeof(kStringReplaceHookerDex);'
    echo ''
    echo '}  // namespace easy_user_hooks::art_demo'
} > "$OUT_HEADER"

rm -rf "$BUILD"
echo "header -> $OUT_HEADER"
