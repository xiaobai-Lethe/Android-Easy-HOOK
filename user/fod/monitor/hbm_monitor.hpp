#pragma once

#include "fod/core/process_context.hpp"
#include "fod/jni/jni_env.hpp"
#include "fod/monitor/monitor_thread.hpp"
#include "fod/udfps/sensor_patcher.hpp"

#include <string_view>

namespace fod::monitor {

/**
 * 2026年08月27日23时18分
 * HBM 监控配置。随 system_server 长期运行，因此 max_attempts 默认为 0 表示不限次数。
 * 2026 BY white
 */
struct HbmMonitorConfig final {
    core::ProcessRole role{core::ProcessRole::kUnknown};
    bool enabled{};
    int max_attempts{0};
    int interval_ms{1000};
    int scheduler_patch_interval{5};
    udfps::SensorPatchConfig sensor_patch;
};

struct HbmMonitorSnapshot final {
    std::string_view current_operation;
    bool has_operation_state{};
    int operation_state{-1};
};

struct HbmMonitorDecision final {
    bool current_is_enroll{};
    bool current_is_authentication{};
    bool state_started{};
    bool active_fingerprint_client{};
    bool should_start_hal{};
};

/**
 * 2026年08月27日23时18分
 * 监控 system_server 中的指纹任务，在认证或录入真正开始时准备 FOD 会话，结束后清理。
 * 实际亮度由内核 FOD 线程控制，本监控只负责让内核进入可点亮的状态。
 * 2026 BY white
 */
class HbmMonitor final {
public:
    HbmMonitor() = delete;

    /**
     * 2026年08月27日23时18分
     * 纯判定函数，不触碰 JNI，可脱离设备单独验证。
     * 只有"任务是认证或录入"且"状态已进入 started"时才认为存在活跃指纹任务。
     * 2026 BY white
     */
    [[nodiscard]] static HbmMonitorDecision Evaluate(const HbmMonitorSnapshot& snapshot) noexcept;

    [[nodiscard]] static MonitorThreadStatus Start(JNIEnv* env, jobject preferred_loader,
                                                   const HbmMonitorConfig& config) noexcept;
};

}  // namespace fod::monitor
