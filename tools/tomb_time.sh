#!/system/bin/sh
# 2026年08月27日22时48分
# 区分 tombstone 新旧：打印每个 system_server 崩溃的时间戳，并与当前时间、开机时长对比。
# 同时确认设备上 app_process64 与 libwhite.so 的现状。
# 2026 BY white

echo "=== 当前时间与开机时长 ==="
date "+now=%Y-%m-%d %H:%M:%S"
cat /proc/uptime

echo
echo "=== 各 system_server tombstone 的时间戳 ==="
for f in /data/tombstones/tombstone_*[0-9]; do
    [ -f "$f" ] || continue
    if grep -q 'Cmdline: system_server' "$f" 2>/dev/null; then
        ts=$(grep -m1 'Timestamp:' "$f")
        echo "  $f  $ts"
    fi
done

echo
echo "=== app_process64 现状（字符串检查）==="
echo -n "含 libeasyhook.so : "; grep -a -c 'libeasyhook.so' /system/bin/app_process64
echo -n "含 libwhite.so    : "; grep -a -c 'libwhite.so' /system/bin/app_process64

echo
echo "=== lib64 中两个 so 是否存在 ==="
ls -l /system/lib64/libeasyhook.so 2>/dev/null || echo "  无 libeasyhook.so"
ls -l /system/lib64/libwhite.so 2>/dev/null || echo "  无 libwhite.so"

echo
echo "=== 当前是否有存活的 zygote / system_server ==="
ps -A 2>/dev/null | grep -E 'zygote|system_server' | grep -v grep || echo "  无存活 zygote/system_server"
