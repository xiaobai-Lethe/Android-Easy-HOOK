#!/bin/bash
#[[
# 2026年08月27日22时26分
# 切换注入点前的预检。核对 libeasyhook.so 的 SONAME 与自身 NEEDED 依赖，
# 确认它的每个依赖都能在目标系统 lib64 中找到，避免 app_process64 早期加载时缺库。
# 2026 BY white
#]]
set -u

ROOT=/mnt/f/AB_DW/MIO-KITCHEN-4.1.8-win/14U
SRC=/mnt/d/PROJECT/Easy-Framework/main/build-android-ninja/libeasyhook.so
LIB64="$ROOT/system/system/lib64"

echo "=== libeasyhook.so SONAME ==="
patchelf --print-soname "$SRC" 2>/dev/null || echo "(无 soname)"

echo
echo "=== libeasyhook.so NEEDED ==="
patchelf --print-needed "$SRC"

echo
echo "=== 每个 NEEDED 在 system/lib64 是否存在 ==="
for dep in $(patchelf --print-needed "$SRC"); do
    if [ -e "$LIB64/$dep" ]; then
        echo "  [有] $dep"
    else
        echo "  [缺] $dep"
    fi
done

echo
echo "=== 对照 libwhite.so 的 SONAME 与 NEEDED ==="
patchelf --print-soname "$LIB64/libwhite.so" 2>/dev/null || echo "(无 soname)"
patchelf --print-needed "$LIB64/libwhite.so"
