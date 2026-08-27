#!/bin/bash
#[[
# 2026年08月27日22时24分
# 检查 14U ROM 中 app_process64 的注入依赖，以及 config 中与 libwhite 相关的权限与
# SELinux 上下文条目，为把注入点从 libwhite.so 切换到 libeasyhook.so 提供依据。
# 2026 BY white
#]]
set -u

ROOT=/mnt/f/AB_DW/MIO-KITCHEN-4.1.8-win/14U
CONFIG="$ROOT/config"

echo "=== app_process64 NEEDED ==="
patchelf --print-needed "$ROOT/system/system/bin/app_process64"

echo
echo "=== system_fs_config: libwhite / app_process64 ==="
grep -nE 'libwhite|app_process64' "$CONFIG/system_fs_config"

echo
echo "=== system_file_contexts: libwhite / app_process64 ==="
grep -nE 'libwhite|app_process64' "$CONFIG/system_file_contexts"

echo
echo "=== 同目录普通 so 参考条目 (libcutils/liblog) ==="
grep -nE 'system/lib64/libcutils\.so|system/lib64/liblog\.so' "$CONFIG/system_fs_config"
grep -nE 'system/lib64/libcutils\.so|system/lib64/liblog\.so' "$CONFIG/system_file_contexts"

echo
echo "=== lib64 下 libwhite.so 现状 ==="
ls -l "$ROOT/system/system/lib64/libwhite.so"

echo
echo "=== sepolicy 规则 (libwhite.te) ==="
cat "$ROOT/white-generic-framework/sepolicy/libwhite.te" 2>/dev/null || echo "(缺失)"
