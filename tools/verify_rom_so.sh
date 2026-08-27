#!/bin/bash
# 2026年08月28日01时58分
# 核对放进镜像的 libeasyhook.so：注入点、权限与 SELinux 条目是否完整，
# 以及 FOD 与 ART 演示两部分代码是否都在产物里。
# 这四项缺任何一件都不会报错，只会静默不生效。
# 2026 BY white

ROM=/mnt/f/AB_DW/MIO-KITCHEN-4.1.8-win/14U
SO="$ROM/system/system/lib64/libeasyhook.so"

echo "=== so ==="
ls -l "$SO"
sha256sum "$SO"

echo
echo "=== app_process64 注入点 ==="
patchelf --print-needed "$ROM/system/system/bin/app_process64" | head -1

echo
echo "=== config 条目 ==="
grep -n 'libeasyhook' "$ROM/config/system_fs_config"
grep -n 'libeasyhook' "$ROM/config/system_file_contexts"

echo
echo "=== 产物内容标记 ==="
for m in 'TEST DEMO JAVA HOOK' 'org.easyhook.demo.StringReplaceHooker' 'art-demo' \
         'EasyHooker_' 'fod-runtime' 'hbm-monitor' 'binder-hook' 'XiaomiFingerprintExtension'; do
    if grep -aq "$m" "$SO"; then
        printf 'FOUND    %s\n' "$m"
    else
        printf 'MISSING  %s\n' "$m"
    fi
done
