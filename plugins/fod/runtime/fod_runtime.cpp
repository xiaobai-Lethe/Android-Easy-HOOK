#include "fod/runtime/fod_runtime.hpp"

#include "easy_hook/core/log.hpp"
#include "fod/monitor/challenge_watchdog.hpp"
#include "fod/monitor/cleanup_watchdog.hpp"
#include "fod/monitor/hbm_monitor.hpp"
#include "fod/core/system_properties.hpp"
#include "fod/udfps/sensor_patcher.hpp"

#include <atomic>
#include <string_view>
#include <unistd.h>

namespace fod::runtime {
namespace {

constexpr std::string_view kLogTag = "fod-runtime";

constexpr std::string_view kEnabledProperty = "persist.easy_hook.fod_enabled";
constexpr std::string_view kSensorIdProperty = "persist.easy_hook.fod_sensor_id";
constexpr std::string_view kLocationProperty = "persist.vendor.sys.fp.fod.location.X_Y";
constexpr std::string_view kSizeProperty = "persist.vendor.sys.fp.fod.size.width_height";

/**
 * 2026年08月27日23时56分
 * 启动相位。fork 出来的子进程会继承这个静态量，因此必须与 pid 一同校验，
 * 否则应用进程会误认为自己已经启动过而跳过初始化。
 * 2026 BY white
 */
std::atomic<pid_t> started_pid{-1};

const char* RoleName(core::ProcessRole role) noexcept {
    switch (role) {
        case core::ProcessRole::kUnknown:
            return "unknown";
        case core::ProcessRole::kOther:
            return "other";
        case core::ProcessRole::kZygote:
            return "zygote";
        case core::ProcessRole::kSystemServer:
            return "system_server";
        case core::ProcessRole::kSettings:
            return "settings";
        case core::ProcessRole::kSystemUi:
            return "systemui";
    }
    return "invalid";
}

/**
 * 2026年08月27日23时56分
 * 启动 system_server 侧的三个监控线程。
 * 三者关注不同故障：清理看门狗解开开机阻塞，挑战看门狗解开设置页卡死，
 * HBM 监控在认证与录入期间管理 FOD 会话。
 * 2026 BY white
 */
void StartSystemServerMonitors(JNIEnv* env, const FodConfig& config) noexcept {
    const udfps::SensorPatchConfig sensor_patch{
        .target_sensor_id = config.target_sensor_id,
        .geometry = config.geometry,
    };

    const auto cleanup_status = monitor::CleanupWatchdog::Start(
        env, nullptr,
        monitor::CleanupWatchdogConfig{
            .role = config.role,
            .enabled = config.enabled,
            .max_attempts = 120,
            .interval_ms = 1000,
            .sensor_patch = sensor_patch,
        });

    const auto challenge_status = monitor::ChallengeWatchdog::Start(
        env, nullptr,
        monitor::ChallengeWatchdogConfig{
            .role = config.role,
            .enabled = config.enabled,
            .max_attempts = 180,
            .interval_ms = 1000,
            .sensor_patch = sensor_patch,
        });

    const auto hbm_status = monitor::HbmMonitor::Start(
        env, nullptr,
        monitor::HbmMonitorConfig{
            .role = config.role,
            .enabled = config.enabled,
            .max_attempts = 0,
            .interval_ms = 1000,
            .scheduler_patch_interval = 5,
            .sensor_patch = sensor_patch,
        });

    easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag,
                           "monitors cleanup=%s challenge=%s hbm=%s",
                           monitor::MonitorThreadStatusName(cleanup_status),
                           monitor::MonitorThreadStatusName(challenge_status),
                           monitor::MonitorThreadStatusName(hbm_status));
}

}  // namespace

const char* FodStartStatusName(FodStartStatus status) noexcept {
    switch (status) {
        case FodStartStatus::kStarted:
            return "started";
        case FodStartStatus::kAlreadyStarted:
            return "already_started";
        case FodStartStatus::kSkippedFeatureDisabled:
            return "skipped_feature_disabled";
        case FodStartStatus::kSkippedProcessRole:
            return "skipped_process_role";
        case FodStartStatus::kMissingJniEnv:
            return "missing_jni_env";
    }
    return "invalid";
}

FodConfig FodConfig::ReadCurrent() noexcept {
    FodConfig config;
    config.role = core::ProcessContext::CurrentRole();
    config.enabled = core::SystemProperties::GetBool(kEnabledProperty, true);
    config.target_sensor_id = core::SystemProperties::GetInt(kSensorIdProperty, 0);
    config.geometry = udfps::Geometry::FromPropertyStrings(
        core::SystemProperties::GetString(kLocationProperty, {}),
        core::SystemProperties::GetString(kSizeProperty, {}));
    return config;
}

FodStartStatus FodRuntime::Start(JNIEnv* env) noexcept {
    if (env == nullptr) {
        return FodStartStatus::kMissingJniEnv;
    }

    const pid_t current_pid = getpid();
    if (started_pid.load(std::memory_order_acquire) == current_pid) {
        return FodStartStatus::kAlreadyStarted;
    }

    const FodConfig config = FodConfig::ReadCurrent();
    if (!config.enabled) {
        started_pid.store(current_pid, std::memory_order_release);
        return FodStartStatus::kSkippedFeatureDisabled;
    }

    /**
     * 2026年08月27日23时56分
     * Settings 与 SystemUI 的按压识别完全依赖 Binder 拦截，拦截在入口处已经装好，
     * 这里不需要再做任何事，直接进入终态。
     * 2026 BY white
     */
    if (config.role != core::ProcessRole::kSystemServer) {
        started_pid.store(current_pid, std::memory_order_release);
        easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag, "role=%s needs no monitor",
                               RoleName(config.role));
        return FodStartStatus::kSkippedProcessRole;
    }

    easy::core::Log::Print(
        easy::core::LogLevel::kInfo, kLogTag,
        "starting role=%s sensor_id=%d center=(%d,%d) radius=%d defaults=(location:%d,size:%d)",
        RoleName(config.role), config.target_sensor_id, config.geometry.center_x,
        config.geometry.center_y, config.geometry.radius,
        config.geometry.used_default_location ? 1 : 0,
        config.geometry.used_default_size ? 1 : 0);

    StartSystemServerMonitors(env, config);
    started_pid.store(current_pid, std::memory_order_release);
    return FodStartStatus::kStarted;
}

}  // namespace fod::runtime
