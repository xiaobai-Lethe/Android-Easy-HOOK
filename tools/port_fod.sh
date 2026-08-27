#!/bin/bash
#[[
# 2026年08月27日22时48分
# 把 WHITE-LOADER 的屏下指纹修复移植到 Easy Hook 的 user 目录。
# 只搬运指纹相关模块，丢弃 AOD 与旧的注入层（runtime/native_hooks 与直接调用 Dobby 的部分），
# 那两处会在移植后由 Easy Hook 的入口宏与动态 hook 接口重写。
# 机械改写内容：命名空间 white_loader 改为 fod，头文件前缀同步改名。
# 2026 BY white
#]]
set -euo pipefail

SRC=/mnt/d/PROJECT/Easy-Framework/WHITE-LOADER-feat-udfps-art-hook
DST=/mnt/d/PROJECT/Easy-Framework/main/user/fod

echo "=== 1. 清理旧的移植产物 ==="
rm -rf "$DST"
mkdir -p "$DST"

echo
echo "=== 2. 搬运指纹相关模块（跳过 aod 与旧注入层）==="
# 需要搬运的模块目录，AOD 与 runtime 不在其中。
MODULES="core jni platform binder pointer udfps vendor monitor"

for module in $MODULES; do
    if [ -d "$SRC/include/white_loader/$module" ]; then
        mkdir -p "$DST/$module"
        cp -r "$SRC/include/white_loader/$module/." "$DST/$module/"
    fi
    if [ -d "$SRC/src/$module" ]; then
        mkdir -p "$DST/$module"
        cp -r "$SRC/src/$module/." "$DST/$module/"
    fi
done

echo
echo "=== 3. 移除不需要的文件 ==="
# binder_proxy_hook 直接调用 Dobby 并自带 RegisterNatives 逻辑，移植后由入口文件重写。
rm -f "$DST/binder/binder_proxy_hook.hpp" "$DST/binder/binder_proxy_hook.cpp"
# core/debug 由 Easy Hook 的日志实现替换，稍后单独手写兼容层。
rm -f "$DST/core/debug.hpp" "$DST/core/debug.cpp"
find "$DST" -name '*.orig' -delete 2>/dev/null || true

echo
echo "=== 4. 命名空间与头文件前缀改写 ==="
find "$DST" -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 |
    xargs -0 sed -i \
        -e 's/white_loader/fod/g' \
        -e 's|#include "fod/|#include "fod/|g'

echo
echo "=== 5. 移植结果 ==="
find "$DST" -type f \( -name '*.hpp' -o -name '*.cpp' \) | sort | sed "s|$DST/|  |"
echo
echo -n "文件总数: "; find "$DST" -type f \( -name '*.hpp' -o -name '*.cpp' \) | wc -l
echo -n "残留 white_loader 引用: "; grep -rl 'white_loader' "$DST" 2>/dev/null | wc -l
