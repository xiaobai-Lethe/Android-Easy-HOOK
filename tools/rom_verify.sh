#!/bin/bash
#[[
# 2026年08月27日22时32分
# 切换注入点后的完整性校验。确认 app_process64 仍是合法 ELF、NEEDED 已指向 libeasyhook.so
# 且其余依赖未被破坏，libeasyhook.so 已就位，两个 config 条目格式正确。
# 2026 BY white
#]]
set -u

ROOT=/mnt/f/AB_DW/MIO-KITCHEN-4.1.8-win/14U
BIN="$ROOT/system/system/bin/app_process64"
LIB64="$ROOT/system/system/lib64"
CONFIG="$ROOT/config"

echo "=== app_process64 ELF 头 ==="
readelf -h "$BIN" 2>/dev/null | grep -E 'Class|Machine|Type'

echo
echo "=== app_process64 NEEDED 全量 ==="
patchelf --print-needed "$BIN"

echo
echo "=== 注入 so 就位情况 ==="
ls -l "$LIB64/libeasyhook.so"

echo
echo "=== libeasyhook.so 依赖在 lib64 的可满足性 ==="
for dep in $(patchelf --print-needed "$LIB64/libeasyhook.so"); do
    if [ -e "$LIB64/$dep" ]; then echo "  [有] $dep"; else echo "  [缺] $dep"; fi
done

echo
echo "=== fs_config 条目 ==="
grep -nE 'system/system/lib64/libeasyhook\.so[[:space:]]' "$CONFIG/system_fs_config"

echo
echo "=== file_contexts 条目 ==="
grep -nE 'libeasyhook\\\.so' "$CONFIG/system_file_contexts"

echo
echo "=== 备份文件 ==="
ls -l "$BIN.easyhook.bak" "$CONFIG/system_fs_config.easyhook.bak" "$CONFIG/system_file_contexts.easyhook.bak"
