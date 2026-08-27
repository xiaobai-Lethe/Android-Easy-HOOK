#include "easy_hook/loader/module_load_watcher.hpp"

#include "easy_hook/core/log.hpp"
#include "easy_hook/core/registry.hpp"

#include <atomic>
#include <mutex>
#include <string_view>

#if defined(__ANDROID__)
#include "dobby.h"

#include <dlfcn.h>
#endif

namespace easy::loader {
namespace {

constexpr std::string_view kLogTag = "module-watcher";

#if defined(__ANDROID__)
constexpr char kDlopenExtSymbol[] = "android_dlopen_ext";

using DlopenExtFunction = void* (*)(const char*, int, const void*);

std::mutex install_mutex;
std::atomic<DlopenExtFunction> original_dlopen_ext{nullptr};
std::atomic_bool watcher_installed{false};

/**
 * 2026年08月27日21时48分
 * android_dlopen_ext 的替换实现。先交由原函数完成真正的加载，再触发注册表对挂起项补装，
 * 这样新模块的段映射与符号表在补装时一定已经就绪。
 * 加载失败时不做补装，避免在每次失败的 dlopen 上做一轮无用的全量解析。
 * 2026 BY white
 */
void* DlopenExtReplacement(const char* filename, int flags, const void* extinfo) {
    const DlopenExtFunction original = original_dlopen_ext.load(std::memory_order_acquire);
    if (original == nullptr) {
        return nullptr;
    }

    void* handle = original(filename, flags, extinfo);
    if (handle == nullptr) {
        return nullptr;
    }

    if (core::Registry::Instance().pending_count() == 0) {
        return handle;
    }

    core::Log::Print(core::LogLevel::kDebug, kLogTag, "module loaded %s, retrying pending hooks",
                     filename != nullptr ? filename : "<null>");
    const core::InstallSummary summary = core::Registry::Instance().RetryPending();
    core::Log::Print(core::LogLevel::kDebug, kLogTag,
                     "retry result installed=%zu pending=%zu failed=%zu", summary.installed,
                     summary.pending, summary.failed);
    return handle;
}
#endif

}  // namespace

bool ModuleLoadWatcher::Install() noexcept {
#if defined(__ANDROID__)
    const std::lock_guard lock(install_mutex);
    if (watcher_installed.load(std::memory_order_acquire)) {
        return true;
    }

    void* target = dlsym(RTLD_DEFAULT, kDlopenExtSymbol);
    if (target == nullptr) {
        target = DobbySymbolResolver("libdl.so", kDlopenExtSymbol);
    }
    if (target == nullptr) {
        core::Log::Print(core::LogLevel::kWarning, kLogTag,
                         "%s is unavailable, pending hooks will not be retried automatically",
                         kDlopenExtSymbol);
        return false;
    }

    void* original = nullptr;
    const int status = DobbyHook(target,
                                 reinterpret_cast<dobby_dummy_func_t>(DlopenExtReplacement),
                                 reinterpret_cast<dobby_dummy_func_t*>(&original));
    if (status != 0 || original == nullptr) {
        core::Log::Print(core::LogLevel::kWarning, kLogTag, "failed to hook %s",
                         kDlopenExtSymbol);
        return false;
    }

    original_dlopen_ext.store(reinterpret_cast<DlopenExtFunction>(original),
                              std::memory_order_release);
    watcher_installed.store(true, std::memory_order_release);
    core::Log::Print(core::LogLevel::kInfo, kLogTag, "watching %s for pending hooks",
                     kDlopenExtSymbol);
    return true;
#else
    return false;
#endif
}

bool ModuleLoadWatcher::installed() noexcept {
#if defined(__ANDROID__)
    return watcher_installed.load(std::memory_order_acquire);
#else
    return false;
#endif
}

}  // namespace easy::loader
