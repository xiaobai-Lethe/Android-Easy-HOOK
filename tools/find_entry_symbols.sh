#!/bin/bash
#[[
# 2026年08月28日00时26分
# 在真实设备库中查找可用的入口符号：
# AndroidRuntime 的启动相关方法，以及 JNI_CreateJavaVM 的实际提供者。
# 2026 BY white
#]]
BIN=/opt/android-ndk-r29/toolchains/llvm/prebuilt/linux-x86_64/bin
DIR=/mnt/d/PROJECT/Easy-Framework/main/tools

echo "=== libandroid_runtime.so 中 AndroidRuntime 的所有方法（动态符号表）==="
"$BIN/llvm-nm" -D --defined-only "$DIR/libandroid_runtime.so" 2>/dev/null |
    grep 'AndroidRuntime' | sed 's/^/  /'

echo
echo "=== 其中含 start / onZygoteInit / onVmCreated 的（含完整符号表）==="
"$BIN/llvm-nm" "$DIR/libandroid_runtime.so" 2>/dev/null |
    grep -E 'AndroidRuntime.*(start|Start|onZygoteInit|onVmCreated|onStarted)' | sed 's/^/  /'

echo
echo "=== 解修饰后的可读形式 ==="
"$BIN/llvm-nm" -C "$DIR/libandroid_runtime.so" 2>/dev/null |
    grep -E 'AndroidRuntime::(start|onZygoteInit|onVmCreated|onStarted)' | sed 's/^/  /'

echo
echo "=== libnativehelper.so 是否导出 JNI_CreateJavaVM ==="
"$BIN/llvm-nm" -D --defined-only "$DIR/libnativehelper.so" 2>/dev/null |
    grep -i 'JNI_' | sed 's/^/  /'

echo
echo "=== libandroid_runtime.so 是否引用 JNI_CreateJavaVM（未定义符号即表示它会调用）==="
"$BIN/llvm-nm" -D --undefined-only "$DIR/libandroid_runtime.so" 2>/dev/null |
    grep -i 'JNI_CreateJavaVM' | sed 's/^/  /'
