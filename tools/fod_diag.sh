#!/system/bin/sh
# 2026年08月28日00时18分
# 屏下指纹修复未生效的诊断：确认 so 是否加载、两个入口 hook 是否命中、运行时是否启动。
# 2026 BY white

echo "=== 1. libeasyhook 加载情况 ==="
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
echo "  合计: $found 个进程"

echo
echo "=== 2. 框架缓存日志 ==="
if [ -f /data/local/tmp/easy_hook.log ]; then
    wc -l /data/local/tmp/easy_hook.log
    echo "--- 最后 60 行 ---"
    tail -60 /data/local/tmp/easy_hook.log
else
    echo "  无 /data/local/tmp/easy_hook.log"
fi

echo
echo "=== 3. logcat 中的 EASY-HOOK ==="
logcat -d -t 3000 2>/dev/null | grep "EASY-HOOK" | tail -60

echo
echo "=== 4. 两个目标符号是否真的存在 ==="
for lib in /system/lib64/libnativehelper.so /apex/com.android.art/lib64/libnativehelper.so; do
    if [ -f "$lib" ]; then
        echo "  $lib :"
        echo -n "    JNI_CreateJavaVM = "
        if grep -a -c 'JNI_CreateJavaVM' "$lib" >/dev/null 2>&1; then
            grep -a -o 'JNI_CreateJavaVM' "$lib" | head -1
        else
            echo "未找到"
        fi
    fi
done
echo -n "  libandroid_runtime.so 中 AndroidRuntime::start 修饰名 = "
grep -a -o '_ZN7android14AndroidRuntime5startEPKcRKNS_6VectorINS_8String8EEEb' \
    /system/lib64/libandroid_runtime.so 2>/dev/null | head -1 || echo "未找到"

echo
echo "=== 5. system_server 是否加载了 libeasyhook ==="
ss_pid=$(pgrep -f system_server 2>/dev/null | head -1)
if [ -n "$ss_pid" ]; then
    echo "  system_server pid=$ss_pid"
    grep -c libeasyhook "/proc/$ss_pid/maps" 2>/dev/null
    echo "  加载的相关库:"
    grep -E 'libeasyhook|libnativehelper|libandroid_runtime' "/proc/$ss_pid/maps" 2>/dev/null | awk '{print $6}' | sort -u | sed 's/^/    /'
else
    echo "  未找到 system_server"
fi
