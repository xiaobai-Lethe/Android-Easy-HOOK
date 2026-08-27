#pragma once

#include <jni.h>

/**
 * 2026年08月28日01时10分
 * ART hook 演示：把系统设置里的"系统更新"文案替换为"TEST DEMO JAVA HOOK"。
 *
 * 这是 Easy Hook 中 ART(Java 方法)hook 的完整示例，与 Dobby 的原生 hook 形成对照：
 * 原生 hook 改写机器指令，而这里改写的是一个运行在 ART 上的 Java 方法的行为。
 *
 * 实现路径：
 *   其一，用框架的 easy::art::ArtHook 初始化 LSPlant，符号解析复用框架已有的
 *         libart 符号表(含 .gnu_debugdata)；
 *   其二，把内嵌的回调类 dex 注入目标进程——注入型框架无法指望目标进程里
 *         存在自己的 Java 类，而 LSPlant 的方法级 hook 又必须要有一个 Java 回调；
 *   其三，hook android.content.res.Resources.getString(int)，
 *         在回调里比对原返回值，命中才替换，其余调用原样放行。
 * 2026 BY white
 */

namespace easy_user_hooks::art_demo {

/**
 * 2026年08月28日01时10分
 * 若当前进程是系统设置，则安装本演示 hook；其他进程直接返回。
 * 进程内只会真正执行一次，重复调用无副作用，因此可以放在任何"JNIEnv 已就绪"的回调里。
 * 实际安装会转到独立线程执行：LSPlant 初始化期间需要挂起全部 Java 线程，
 * 在 Binder 事务的调用栈上直接做这件事有死锁风险。
 * 2026 BY white
 */
void MaybeInstallInSettings(JNIEnv* env) noexcept;

}  // namespace easy_user_hooks::art_demo
