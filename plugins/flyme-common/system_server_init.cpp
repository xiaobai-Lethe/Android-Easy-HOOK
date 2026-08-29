#include "flyme-common/system_server_init.hpp"

#include "easy_hook/easy_hook.hpp"

#if defined(__ANDROID__)

#include <jni.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <unistd.h>

namespace flyme_common {
namespace {

// 注册者上限。目前 AOD、vibrator 两个，留足余量。
constexpr std::size_t kMaxCallbacks = 8;

std::array<SystemServerInitCallback, kMaxCallbacks> g_callbacks{};
std::atomic<std::size_t> g_count{0};

// 按 pid 保证每个进程只分发一次(zygote fork 出的子进程会继承静态量)。
std::atomic<pid_t> g_dispatched_pid{-1};

void DispatchOnce(JNIEnv* env) noexcept {
    const pid_t current = getpid();
    pid_t expected = g_dispatched_pid.load(std::memory_order_acquire);
    if (expected == current) {
        return;
    }
    if (!g_dispatched_pid.compare_exchange_strong(expected, current,
                                                  std::memory_order_acq_rel)) {
        return;
    }

    const std::size_t count = g_count.load(std::memory_order_acquire);
    for (std::size_t index = 0; index < count && index < kMaxCallbacks; ++index) {
        const SystemServerInitCallback callback = g_callbacks[index];
        if (callback != nullptr) {
            callback(env);
        }
    }
}

}  // namespace

void RegisterSystemServerInit(SystemServerInitCallback callback) noexcept {
    if (callback == nullptr) {
        return;
    }
    const std::size_t index = g_count.fetch_add(1, std::memory_order_acq_rel);
    if (index < kMaxCallbacks) {
        g_callbacks[index] = callback;
    }
}

}  // namespace flyme_common

/**
 * 全框架唯一的 libandroid_servers.so JNI_OnLoad hook：只在 system_server 命中。
 * 先跑原实现(否则 system_server 的原生方法不注册会崩)，再分发给所有注册者。
 * 不要在别处再 hook 这个符号，需要 system_server 入口就用 RegisterSystemServerInit 注册。
 */
EASY_HOOK_SYMBOL(FlymeCommonServerInit, "libandroid_servers.so", "JNI_OnLoad", jint, JavaVM* vm,
                 void* reserved) {
    const jint result = EASY_HAS_ORIGINAL() ? EASY_ORIGINAL(vm, reserved) : JNI_VERSION_1_6;

    JNIEnv* env = nullptr;
    if (vm != nullptr) {
        vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    }
    if (env != nullptr) {
        flyme_common::DispatchOnce(env);
    }
    return result;
}

#endif  // __ANDROID__
