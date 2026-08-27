#!/bin/bash
#[[
# 2026年08月28日00时08分
# 校验移植后的产物：确认指纹模块与两个入口 hook 都已链入，且自注册对象存在。
# 2026 BY white
#]]
NM=/opt/android-ndk-r29/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm
SO=/mnt/d/PROJECT/Easy-Framework/main/build-android-ninja/libeasyhook.so

"$NM" -C "$SO" > /tmp/fod_syms.txt 2>&1

echo "=== 用户声明的 hook 自注册对象 ==="
grep 'easy_user_hooks::.*kRegistration' /tmp/fod_syms.txt | sed 's/^/  /'

echo
echo "=== 入口替换函数 ==="
grep 'easy_user_hooks::.*::Replacement' /tmp/fod_syms.txt | sed 's/^/  /'

echo
echo "=== 指纹各层代表符号 ==="
for pattern in \
    'fod::runtime::FodRuntime::Start' \
    'fod::binder::InstallRegisterNativesHook' \
    'fod::monitor::HbmMonitor::Start' \
    'fod::monitor::CleanupWatchdog::Start' \
    'fod::monitor::ChallengeWatchdog::Start' \
    'fod::monitor::StartMonitorThread' \
    'fod::fingerprint::ServiceAccess::ResolveScheduler' \
    'fod::jni::DescribeObject' \
    'fod::udfps::SchedulerPatcher::PatchJavaService' \
    'fod::vendor::FodSession::Arm' \
    'fod::core::ProcessContext::CurrentRole'
do
    if grep -q "$pattern" /tmp/fod_syms.txt; then
        echo "  [有] $pattern"
    else
        echo "  [缺] $pattern"
    fi
done

echo
echo "=== 动态 hook 接口 ==="
grep -E 'easy::HookFunction|easy::UnhookFunction' /tmp/fod_syms.txt | sed 's/^/  /'

echo
echo "=== 产物依赖 ==="
patchelf --print-needed "$SO"
