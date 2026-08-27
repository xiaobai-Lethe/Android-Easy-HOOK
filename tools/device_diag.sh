#!/system/bin/sh
# 2026年08月27日22时40分
# 设备侧诊断：确认 libeasyhook.so 是否加载、构造函数是否运行、system_server 崩溃原因。
# 2026 BY white

echo "=== 1. 我的缓存日志文件（存在说明构造函数与日志已运行）==="
ls -l /data/local/tmp/easy_hook.log 2>/dev/null || echo "NO easy_hook.log"

echo
echo "=== 2. libeasyhook 是否被存活进程加载 ==="
found=0
for p in $(ls /proc 2>/dev/null); do
    case "$p" in
        ''|*[!0-9]*) continue ;;
    esac
    if grep -q libeasyhook "/proc/$p/maps" 2>/dev/null; then
        name=$(cat "/proc/$p/cmdline" 2>/dev/null | tr '\0' ' ')
        echo "  pid $p : $name"
        found=$((found + 1))
    fi
done
echo "  加载 libeasyhook 的进程数: $found"

echo
echo "=== 3. app_process64 是否确实以 libeasyhook 为依赖 ==="
grep -a -o 'libeasyhook.so' /system/bin/app_process64 | head -1 || echo "未在 app_process64 中发现字符串"

echo
echo "=== 4. system_server tombstone 的 Abort message 汇总 ==="
grep -h 'Abort message' /data/tombstones/tombstone_*[0-9] 2>/dev/null | sort | uniq -c

echo
echo "=== 5. 含 libeasyhook 的 tombstone（若有说明崩在我代码里）==="
grep -l libeasyhook /data/tombstones/tombstone_*[0-9] 2>/dev/null || echo "  无 tombstone 提及 libeasyhook"

echo
echo "=== 6. logcat 中 EASY-HOOK 标签（近期）==="
logcat -d -t 400 2>/dev/null | grep -i "EASY-HOOK" | head -20 || echo "  logcat 无 EASY-HOOK"
echo "(诊断完成)"
