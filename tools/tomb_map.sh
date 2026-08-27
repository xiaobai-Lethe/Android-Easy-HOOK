#!/system/bin/sh
# 2026年08月27日22时44分
# 把每个 tombstone 的进程名与 abort message 一一对应，消除编号滚动干扰，定位 system_server 真正死因。
# 2026 BY white

for f in /data/tombstones/tombstone_*[0-9]; do
    [ -f "$f" ] || continue
    cmd=$(grep -m1 'Cmdline:' "$f" 2>/dev/null | sed 's/Cmdline: //')
    abort=$(grep -m1 'Abort message:' "$f" 2>/dev/null)
    sig=$(grep -m1 'signal ' "$f" 2>/dev/null)
    echo "### $f"
    echo "    proc : $cmd"
    echo "    $sig"
    echo "    $abort"
done

echo
echo "=== system_server tombstone 的前若干栈帧 ==="
for f in /data/tombstones/tombstone_*[0-9]; do
    [ -f "$f" ] || continue
    if grep -q 'Cmdline: system_server' "$f" 2>/dev/null; then
        echo "### $f"
        grep -m1 'Abort message:' "$f"
        grep -A12 '^backtrace:' "$f" | head -13
        break
    fi
done
