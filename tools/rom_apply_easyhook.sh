#!/bin/bash
#[[
# 2026年08月27日22时30分
# 把 14U ROM 的启动注入点从 libwhite.so 切换到 libeasyhook.so。
# 步骤：备份 -> 放入 so -> 用 patchelf 重定向 app_process64 的 NEEDED ->
# 追加 fs_config 权限条目 -> 追加 file_contexts 的 SELinux 上下文条目。
# 全流程幂等，重复执行不会产生重复条目，且在关键步骤后立即校验。
# 2026 BY white
#]]
set -euo pipefail

ROOT=/mnt/f/AB_DW/MIO-KITCHEN-4.1.8-win/14U
SRC=/mnt/d/PROJECT/Easy-Framework/main/build-android-ninja/libeasyhook.so
BIN="$ROOT/system/system/bin/app_process64"
LIB64="$ROOT/system/system/lib64"
CONFIG="$ROOT/config"
FS_CONFIG="$CONFIG/system_fs_config"
FILE_CONTEXTS="$CONFIG/system_file_contexts"

OLD_NAME=libwhite.so
NEW_NAME=libeasyhook.so

echo "=== 0. 预检 ==="
for path in "$SRC" "$BIN" "$LIB64" "$FS_CONFIG" "$FILE_CONTEXTS"; do
    if [ ! -e "$path" ]; then
        echo "缺少必要路径: $path" >&2
        exit 1
    fi
done
echo "预检通过"

echo
echo "=== 1. 备份原始文件 ==="
for target in "$BIN" "$FS_CONFIG" "$FILE_CONTEXTS"; do
    if [ ! -e "$target.easyhook.bak" ]; then
        cp -a "$target" "$target.easyhook.bak"
        echo "已备份 $target -> $target.easyhook.bak"
    else
        echo "备份已存在，跳过 $target.easyhook.bak"
    fi
done

echo
echo "=== 2. 放入 libeasyhook.so ==="
cp -f "$SRC" "$LIB64/$NEW_NAME"
chmod 0644 "$LIB64/$NEW_NAME"
ls -l "$LIB64/$NEW_NAME"

echo
echo "=== 3. 重定向 app_process64 的 NEEDED ==="
if patchelf --print-needed "$BIN" | grep -qx "$NEW_NAME"; then
    echo "NEEDED 已是 $NEW_NAME，跳过"
elif patchelf --print-needed "$BIN" | grep -qx "$OLD_NAME"; then
    patchelf --replace-needed "$OLD_NAME" "$NEW_NAME" "$BIN"
    echo "已把 $OLD_NAME 替换为 $NEW_NAME"
else
    echo "app_process64 未依赖 $OLD_NAME，无法定位注入点" >&2
    exit 1
fi
echo "当前 NEEDED 首项："
patchelf --print-needed "$BIN" | head -1

echo
echo "=== 4. 追加 fs_config 权限条目 ==="
if grep -qE "system/system/lib64/${NEW_NAME}[[:space:]]" "$FS_CONFIG"; then
    echo "fs_config 已含 $NEW_NAME 条目，跳过"
else
    awk -v old="system/system/lib64/${OLD_NAME}" \
        -v new="system/system/lib64/${NEW_NAME} 0 0 0644" '
        { print }
        $1 == old { print new }
    ' "$FS_CONFIG" > "$FS_CONFIG.tmp"
    mv "$FS_CONFIG.tmp" "$FS_CONFIG"
    echo "已追加 fs_config 条目"
fi
grep -nE "system/system/lib64/(libwhite|libeasyhook)\.so[[:space:]]" "$FS_CONFIG"

echo
echo "=== 5. 追加 file_contexts 的 SELinux 上下文条目 ==="
if grep -qE "libeasyhook\\\\?\.so" "$FILE_CONTEXTS"; then
    echo "file_contexts 已含 $NEW_NAME 条目，跳过"
else
    awk '
        { print }
        /^\/system\/system\/lib64\/libwhite\\\.so[[:space:]]/ {
            line = $0
            sub(/libwhite/, "libeasyhook", line)
            print line
        }
    ' "$FILE_CONTEXTS" > "$FILE_CONTEXTS.tmp"
    mv "$FILE_CONTEXTS.tmp" "$FILE_CONTEXTS"
    echo "已追加 file_contexts 条目"
fi
grep -nE "libwhite\\\\?\.so|libeasyhook\\\\?\.so" "$FILE_CONTEXTS"

echo
echo "=== 完成 ==="
