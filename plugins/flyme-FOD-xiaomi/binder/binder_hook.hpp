#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"

namespace fod::binder {

/**
 * 2026年08月27日23时48分
 * 进程初始化回调。BinderProxy 的原生事务方法是本框架在各进程中第一个稳定的执行点，
 * 因此把"运行时是否该启动"的判断挂在它上面。
 * 2026 BY white
 */
using PostForkCallback = void (*)(JNIEnv* env) noexcept;

void SetPostForkCallback(PostForkCallback callback) noexcept;

/**
 * 2026年08月27日23时48分
 * 拦截 JNI 的 RegisterNatives，用于取得 BinderProxy.transactNative 的真实函数地址。
 * 该方法由运行时在类初始化时动态注册，没有导出符号，只能这样拿到。
 *
 * 找到目标并装上 transactNative 拦截后，本替身即退化为廉价透传（一次原子判断再转发），
 * 因此可长期保留；若要彻底移除，须在非事务上下文调用 UninstallRegisterNativesHook，
 * 不能在替身内部自拆。真正驱动指纹修复的是 transactNative 拦截，它全程不会被撤除。
 * 2026 BY white
 */
[[nodiscard]] bool InstallRegisterNativesHook(JNIEnv* env) noexcept;

/**
 * 2026年08月27日23时48分
 * 撤除 RegisterNatives 拦截。它的任务只是找到目标一次，长期保留会介入后续所有原生方法注册；
 * 若被 zygote 的子进程继承，还会干扰它们的类加载。即使尚未安装也可安全调用。
 * 2026 BY white
 */
void UninstallRegisterNativesHook() noexcept;

/**
 * 2026年08月27日23时48分
 * 返回 transactNative 的原始函数地址。
 * 补发指针抬起事务时必须走原始实现，否则会再次进入本框架的拦截而形成自我递归。
 * 2026 BY white
 */
[[nodiscard]] void* OriginalTransact() noexcept;

}  // namespace fod::binder
