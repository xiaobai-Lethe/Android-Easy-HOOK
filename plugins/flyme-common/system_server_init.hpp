#pragma once

/**
 * 2026年08月29日
 * system_server 初始化分发器。
 *
 * 多个"只在 system_server 生效"的插件都想拿同一个原生入口 libandroid_servers.so 的
 * JNI_OnLoad，但 inline hook 同一地址只能装一次(Dobby 会拒绝第二个)。
 * 因此这里只 hook 一次 JNI_OnLoad，再把它分发给所有注册者。
 *
 * 用法(在插件的某个 .cpp 里，文件作用域)：
 *   namespace { void OnServerInit(JNIEnv* env) noexcept { ...你的安装逻辑... } }
 *   const flyme_common::SystemServerInitRegistrar kReg{&OnServerInit};
 * 2026 BY white
 */

#if defined(__ANDROID__)
#include <jni.h>
#else
struct JNIEnv;
#endif

namespace flyme_common {

using SystemServerInitCallback = void (*)(JNIEnv* env) noexcept;

/**
 * 注册一个回调，在 system_server 的 libandroid_servers.so JNI_OnLoad 时(每进程一次)被调用。
 * 可在静态初始化期安全调用；注册顺序不敏感。
 */
void RegisterSystemServerInit(SystemServerInitCallback callback) noexcept;

// 静态初始化期自注册的便捷包装。
struct SystemServerInitRegistrar final {
    explicit SystemServerInitRegistrar(SystemServerInitCallback callback) noexcept {
        RegisterSystemServerInit(callback);
    }
};

}  // namespace flyme_common
