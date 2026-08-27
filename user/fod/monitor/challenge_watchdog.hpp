#pragma once

#include "fod/core/process_context.hpp"
#include "fod/jni/jni_env.hpp"
#include "fod/monitor/monitor_thread.hpp"
#include "fod/udfps/sensor_patcher.hpp"

#include <string_view>

namespace fod::monitor {

/**
 * 2026年08月27日23时30分
 * 挑战看门狗配置。generateChallenge 卡住只发生在进入指纹设置的场景，
 * 因此使用有限次数运行。
 * 2026 BY white
 */
struct ChallengeWatchdogConfig final {
    core::ProcessRole role{core::ProcessRole::kUnknown};
    bool enabled{};
    int max_attempts{180};
    int interval_ms{1000};
    udfps::SensorPatchConfig sensor_patch;
};

struct ChallengeWatchdogSnapshot final {
    std::string_view current_operation;
};

/**
 * 2026年08月27日23时30分
 * 三项条件同时成立才视为卡住，避免把正常排队误判为故障。
 * 2026 BY white
 */
struct ChallengeWatchdogDecision final {
    bool current_is_generate{};
    bool owner_is_settings{};
    bool state_started{};
    bool is_stuck_candidate{};
};

/**
 * 2026年08月27日23时30分
 * 检测已启动但 HAL 长时间不返回的 generateChallenge，并重新触发一次 HAL 调用。
 * 该任务卡住时指纹设置页会一直停在加载状态。
 * 2026 BY white
 */
class ChallengeWatchdog final {
public:
    ChallengeWatchdog() = delete;

    /**
     * 2026年08月27日23时30分
     * 需要连续多轮观察到同一卡住特征才动手。
     * 正常的 generateChallenge 也会短暂处于 started 状态，一次命中就重试会误伤正常流程。
     * 2026 BY white
     */
    static constexpr int kRequiredStableObservations = 5;

    [[nodiscard]] static ChallengeWatchdogDecision Evaluate(
        const ChallengeWatchdogSnapshot& snapshot) noexcept;

    [[nodiscard]] static MonitorThreadStatus Start(
        JNIEnv* env, jobject preferred_loader, const ChallengeWatchdogConfig& config) noexcept;
};

}  // namespace fod::monitor
