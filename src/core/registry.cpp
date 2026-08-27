#include "easy_hook/core/registry.hpp"

#include "easy_hook/core/engine.hpp"
#include "easy_hook/core/log.hpp"
#include "easy_hook/core/process_filter.hpp"

#include <string_view>

namespace easy::core {
namespace {

constexpr std::string_view kLogTag = "registry";

/**
 * 2026年08月27日22时38分
 * 同线程安装重入标记。安装过程中解析符号或改写代码都可能触发模块加载，
 * 而模块加载监听又会回调注册表要求补装挂起项。若不拦截，这条回路会在同一线程上
 * 递归进入安装流程并在安装互斥量上自锁。检测到重入时直接放弃本次补装，
 * 外层那轮安装本来就会覆盖这些注册项，不会漏装。
 * 2026 BY white
 */
thread_local bool installing_on_this_thread = false;

/**
 * 2026年08月27日22时38分
 * 安装重入标记的 RAII 守卫，保证任何返回路径都会复位标记。
 * 2026 BY white
 */
class InstallGuard final {
public:
    InstallGuard() noexcept { installing_on_this_thread = true; }
    ~InstallGuard() { installing_on_this_thread = false; }

    InstallGuard(const InstallGuard&) = delete;
    InstallGuard& operator=(const InstallGuard&) = delete;
};

/**
 * 2026年08月27日21时48分
 * 判断进程名 token 是否匹配允许项。支持精确匹配以及 "包名:子进程" 形式的前缀匹配，
 * 使 com.example 能覆盖 com.example 与 com.example:remote 等子进程。
 * 2026 BY white
 */
bool ProcessTokenMatches(std::string_view token, std::string_view allowed) noexcept {
    if (allowed.empty()) {
        return false;
    }
    if (token == allowed) {
        return true;
    }
    return token.size() > allowed.size() && token.starts_with(allowed) &&
           token[allowed.size()] == ':';
}

void AccumulateSummary(InstallSummary& summary, InstallState state) noexcept {
    switch (state) {
        case InstallState::kInstalled:
            ++summary.installed;
            return;
        case InstallState::kModuleNotLoaded:
            ++summary.pending;
            return;
        case InstallState::kSkippedProcessFilter:
            ++summary.skipped;
            return;
        case InstallState::kNotAttempted:
            return;
        case InstallState::kInvalidSpec:
        case InstallState::kSymbolNotFound:
        case InstallState::kOffsetOutOfRange:
        case InstallState::kPatternInvalid:
        case InstallState::kPatternNotFound:
        case InstallState::kPatternAmbiguous:
        case InstallState::kBackendFailed:
            ++summary.failed;
            return;
    }
}

}  // namespace

Registry& Registry::Instance() noexcept {
    static Registry instance;
    return instance;
}

void Registry::Add(Registration* registration) noexcept {
    if (registration == nullptr) {
        return;
    }

    bool install_now = false;
    {
        const std::lock_guard lock(mutex_);
        registrations_.push_back(registration);
        install_now = install_active_;
    }

    /**
     * 2026年08月27日22时38分
     * 运行时已启动说明本注册项的静态初始化排在库入口构造函数之后，
     * 首轮安装已经结束，必须就地补装，否则这个 hook 会一直停在未尝试状态。
     * 安装在锁外进行：安装可能触发模块加载并回调注册表，持锁会造成同线程自锁。
     * 2026 BY white
     */
    if (!install_now || installing_on_this_thread) {
        return;
    }

    const InstallGuard guard;
    const std::lock_guard serialize(install_mutex_);
    HookEngine::Instance().Install(*registration);
}

void Registry::SetInstallActive(bool active) noexcept {
    const std::lock_guard lock(mutex_);
    install_active_ = active;
}

bool Registry::install_active() const noexcept {
    const std::lock_guard lock(mutex_);
    return install_active_;
}

void Registry::AddAllowedProcess(const char* process_name) noexcept {
    if (process_name == nullptr || process_name[0] == '\0') {
        return;
    }
    const std::lock_guard lock(mutex_);
    allowed_processes_.emplace_back(process_name);
}

bool Registry::IsCurrentProcessAllowed() const noexcept {
    const std::lock_guard lock(mutex_);
    if (allowed_processes_.empty()) {
        return true;
    }

    const std::string_view token(CurrentProcessName());
    for (const std::string& allowed : allowed_processes_) {
        if (ProcessTokenMatches(token, allowed)) {
            return true;
        }
    }
    return false;
}

InstallSummary Registry::InstallAll() noexcept {
    return RunInstall(false);
}

InstallSummary Registry::RetryPending() noexcept {
    return RunInstall(true);
}

InstallSummary Registry::RunInstall(bool retry_only) noexcept {
    if (installing_on_this_thread) {
        return InstallSummary{};
    }

    std::vector<Registration*> snapshot;
    {
        const std::lock_guard lock(mutex_);
        snapshot = registrations_;
    }

    InstallSummary summary;
    summary.total = snapshot.size();

    const InstallGuard guard;
    const std::lock_guard serialize(install_mutex_);

    HookEngine& engine = HookEngine::Instance();
    for (Registration* registration : snapshot) {
        if (registration == nullptr) {
            continue;
        }

        const InstallState current = registration->state();
        if (current == InstallState::kInstalled) {
            ++summary.installed;
            continue;
        }
        if (retry_only && !IsRetryableState(current)) {
            AccumulateSummary(summary, current);
            continue;
        }

        engine.Install(*registration);
        AccumulateSummary(summary, registration->state());
    }
    return summary;
}

void Registry::DumpStatus() const noexcept {
    const std::lock_guard lock(mutex_);
    Log::Print(LogLevel::kInfo, kLogTag, "registered hooks total=%zu", registrations_.size());
    for (const Registration* registration : registrations_) {
        if (registration == nullptr) {
            continue;
        }
        const TargetSpec& spec = registration->spec();
        Log::Print(LogLevel::kInfo, kLogTag,
                   "hook name=%s kind=%s module=%s state=%s address=%p",
                   spec.name != nullptr ? spec.name : "?", TargetKindName(spec.kind),
                   spec.module != nullptr ? spec.module : "?",
                   InstallStateName(registration->state()),
                   reinterpret_cast<void*>(registration->resolved_address()));
    }
}

std::size_t Registry::size() const noexcept {
    const std::lock_guard lock(mutex_);
    return registrations_.size();
}

std::size_t Registry::pending_count() const noexcept {
    const std::lock_guard lock(mutex_);
    std::size_t pending = 0;
    for (const Registration* registration : registrations_) {
        if (registration != nullptr && IsRetryableState(registration->state()) &&
            registration->state() != InstallState::kNotAttempted) {
            ++pending;
        }
    }
    return pending;
}

}  // namespace easy::core
