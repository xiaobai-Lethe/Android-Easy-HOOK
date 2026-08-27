#pragma once

#include "fod/core/process_context.hpp"
#include "fod/jni/jni_env.hpp"
#include "fod/monitor/monitor_thread.hpp"
#include "fod/udfps/sensor_patcher.hpp"

#include <string_view>

namespace fod::monitor {

/**
 * 2026年08月27日23时24分
 * 清理看门狗配置。internalCleanup 卡住只会发生在开机后的一段时间内，
 * 因此使用有限次数而非长期运行。
 * 2026 BY white
 */
struct CleanupWatchdogConfig final {
    core::ProcessRole role{core::ProcessRole::kUnknown};
    bool enabled{};
    int max_attempts{120};
    int interval_ms{1000};
    udfps::SensorPatchConfig sensor_patch;
};

struct CleanupWatchdogSnapshot final {
    std::string_view current_operation;
    std::string_view pending_operations;
};

/**
 * 2026年08月27日23时24分
 * 是否应当取消当前清理任务的判定结果。
 * 只有确认当前任务确实是清理操作，且它挡住了后续指纹任务时才允许取消。
 * 2026 BY white
 */
struct CleanupWatchdogDecision final {
    bool current_is_cleanup{};
    bool pending_is_empty{};
    bool has_settings_pending{};
    bool has_fingerprint_pending{};
    bool should_cancel{};
};

/**
 * 2026年08月27日23时24分
 * 检测长期占用调度器的 internalCleanup 并取消它。
 * 该任务卡住时后续所有指纹操作都无法开始，表现为指纹功能完全无响应。
 * 2026 BY white
 */
class CleanupWatchdog final {
public:
    CleanupWatchdog() = delete;

    [[nodiscard]] static CleanupWatchdogDecision Evaluate(
        const CleanupWatchdogSnapshot& snapshot) noexcept;

    [[nodiscard]] static MonitorThreadStatus Start(JNIEnv* env, jobject preferred_loader,
                                                   const CleanupWatchdogConfig& config) noexcept;
};

}  // namespace fod::monitor
