#include "easy_hook/core/runtime.hpp"

#include "easy_hook/core/log.hpp"
#include "easy_hook/core/process_filter.hpp"
#include "easy_hook/loader/module_load_watcher.hpp"

#include <atomic>
#include <string_view>

namespace easy::core {
namespace {

constexpr std::string_view kLogTag = "runtime";

std::atomic_bool runtime_started{false};

}  // namespace

const char* Runtime::StartStatusName(RuntimeStartStatus status) noexcept {
    switch (status) {
        case RuntimeStartStatus::kStarted:
            return "started";
        case RuntimeStartStatus::kAlreadyStarted:
            return "already_started";
        case RuntimeStartStatus::kSkippedProcessFilter:
            return "skipped_process_filter";
        case RuntimeStartStatus::kNoRegistration:
            return "no_registration";
    }
    return "invalid";
}

RuntimeStartStatus Runtime::Start() noexcept {
    bool expected = false;
    if (!runtime_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
        return RuntimeStartStatus::kAlreadyStarted;
    }

    Log::InitializeFromProperties();

    Registry& registry = Registry::Instance();
    if (!registry.IsCurrentProcessAllowed()) {
        Log::Print(LogLevel::kInfo, kLogTag, "process %s is filtered out, no hook installed",
                   CurrentProcessName());
        return RuntimeStartStatus::kSkippedProcessFilter;
    }

    /**
     * 2026年08月27日22时10分
     * 在首轮安装之前开启安装开关，使静态初始化排在本函数之后的 hook 在登记时就地安装。
     * 库入口构造函数与用户 hook 的静态初始化同处 .init_array，先后顺序由链接顺序决定，
     * 两种顺序都必须能正确工作，不能依赖当前的链接布局。
     * 2026 BY white
     */
    registry.SetInstallActive(true);

    const InstallSummary summary = registry.InstallAll();

    /**
     * 2026年08月27日22时40分
     * 只有确实存在等待目标模块的 hook 时才安装模块加载监听。
     * 该监听会介入本进程后续每一次动态库加载，而本框架被注入到 zygote 后会存在于所有进程，
     * 没有挂起项时保留它只会平白扩大影响面和风险。
     * 2026 BY white
     */
    const bool watcher_installed =
        summary.pending > 0 ? loader::ModuleLoadWatcher::Install() : false;
    Log::Print(LogLevel::kInfo, kLogTag,
               "process=%s watcher=%d total=%zu installed=%zu pending=%zu failed=%zu",
               CurrentProcessName(), watcher_installed ? 1 : 0, summary.total,
               summary.installed, summary.pending, summary.failed);

    if (summary.pending > 0 && !watcher_installed) {
        Log::Print(LogLevel::kWarning, kLogTag,
                   "%zu hooks are pending but the module watcher is unavailable; "
                   "call easy::InstallAll() manually after the target module is loaded",
                   summary.pending);
    }

    registry.DumpStatus();
    return summary.total == 0 ? RuntimeStartStatus::kNoRegistration
                              : RuntimeStartStatus::kStarted;
}

}  // namespace easy::core
