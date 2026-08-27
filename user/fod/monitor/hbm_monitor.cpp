#include "fod/monitor/hbm_monitor.hpp"

#include "fod/core/debug.hpp"
#include "fod/fingerprint/service_access.hpp"
#include "fod/jni/reflect.hpp"
#include "fod/udfps/scheduler_patcher.hpp"
#include "fod/vendor/aidl_int_transaction.hpp"
#include "fod/vendor/fod_session.hpp"

#include <memory>
#include <new>
#include <string_view>

namespace fod::monitor {
namespace {

constexpr std::string_view kLogTag = "hbm-monitor";
constexpr std::string_view kEnrollClient = "FingerprintEnrollClient";
constexpr std::string_view kAuthenticationClient = "FingerprintAuthenticationClient";
constexpr std::string_view kStartedStateText = "State: 2";
constexpr int kStartedStateValue = 2;

bool Contains(std::string_view text, std::string_view value) noexcept {
    return !value.empty() && text.find(value) != std::string_view::npos;
}

/**
 * 2026年08月27日23时20分
 * HBM 监控任务。每轮读取调度器当前任务，据此决定是否准备或清理 FOD 会话，
 * 并按配置的间隔重新应用一次传感器与调度器补丁。
 * 2026 BY white
 */
class HbmMonitorTask final : public MonitorTask {
public:
    explicit HbmMonitorTask(const HbmMonitorConfig& config) noexcept : config_(config) {}

    [[nodiscard]] bool Tick(JNIEnv* env, jobject preferred_loader, int attempt) noexcept override {
        PatchSchedulerIfDue(env, preferred_loader, attempt);
        Apply(env, preferred_loader);
        return true;
    }

    /**
     * 2026年08月27日23时20分
     * 监控结束时若仍处于已准备状态，必须关闭触控模式，
     * 否则触控固件会一直停留在 FOD 模式影响正常触摸。
     * 2026 BY white
     */
    void Finish(JNIEnv* env, jobject preferred_loader) noexcept override {
        static_cast<void>(preferred_loader);
        if (!forced_hal_active_) {
            return;
        }
        vendor::JniAidlIntTransactionBackend backend(env);
        vendor::FodSession session(backend);
        static_cast<void>(session.SetTouchMode(false));
        forced_hal_active_ = false;
    }

private:
    /**
     * 2026年08月27日23时20分
     * 传感器与调度器的字段会被系统在运行中重新赋值，因此需要周期性重新应用，
     * 而不是只在启动时补一次。
     * 2026 BY white
     */
    void PatchSchedulerIfDue(JNIEnv* env, jobject preferred_loader, int attempt) noexcept {
        if (config_.scheduler_patch_interval <= 0 ||
            attempt % config_.scheduler_patch_interval != 0) {
            return;
        }

        fingerprint::ServiceAccess access(env, preferred_loader);
        jni::LocalRefScope scope(env);
        jobject service = scope.Track(access.ResolveService());
        if (service == nullptr) {
            return;
        }
        static_cast<void>(udfps::SchedulerPatcher::PatchJavaService(env, service,
                                                                    config_.sensor_patch));
    }

    void Apply(JNIEnv* env, jobject preferred_loader) noexcept {
        fingerprint::ServiceAccess access(env, preferred_loader);
        jni::LocalRefScope scope(env);

        jobject service = scope.Track(access.ResolveService());
        if (service == nullptr) {
            return;
        }
        jobject scheduler = scope.Track(access.ResolveScheduler(service));
        if (scheduler == nullptr) {
            return;
        }

        jobject operation = scope.Track(access.ResolveCurrentOperation(scheduler));
        if (operation == nullptr) {
            /**
             * 2026年08月27日23时20分
             * 调度器空闲说明上一个指纹任务已经结束。此时若还停留在已准备状态，
             * 必须立即清理，否则触控模式会一直挂着。
             * 2026 BY white
             */
            ResetWithoutOperation(env);
            return;
        }

        const jni::ObjectDescription description = access.Describe(operation);
        if (description.status != jni::ObjectDescriptionStatus::kDescribed) {
            return;
        }

        int operation_state = -1;
        const bool has_state = access.ReadOperationState(operation, operation_state);
        const HbmMonitorDecision decision = HbmMonitor::Evaluate({
            .current_operation = description.text,
            .has_operation_state = has_state,
            .operation_state = operation_state,
        });

        if (decision.should_start_hal == forced_hal_active_) {
            return;
        }

        vendor::JniAidlIntTransactionBackend backend(env);
        vendor::FodSession session(backend);
        session.Arm(decision.should_start_hal);
        const vendor::AidlIntTransactionResult touch =
            session.SetTouchMode(decision.should_start_hal);
        forced_hal_active_ = decision.should_start_hal;

        core::Debug::Log(core::LogLevel::kInfo, kLogTag,
                         "fod session %s ok=%d state=%d enroll=%d auth=%d",
                         decision.should_start_hal ? "armed" : "disarmed", touch.ok ? 1 : 0,
                         operation_state, decision.current_is_enroll ? 1 : 0,
                         decision.current_is_authentication ? 1 : 0);
    }

    void ResetWithoutOperation(JNIEnv* env) noexcept {
        if (!forced_hal_active_) {
            return;
        }
        vendor::JniAidlIntTransactionBackend backend(env);
        vendor::FodSession session(backend);
        session.Arm(false);
        static_cast<void>(session.SetTouchMode(false));
        forced_hal_active_ = false;
        core::Debug::Log(core::LogLevel::kInfo, kLogTag,
                         "fod session disarmed because scheduler has no current operation");
    }

    HbmMonitorConfig config_;
    bool forced_hal_active_{false};
};

}  // namespace

HbmMonitorDecision HbmMonitor::Evaluate(const HbmMonitorSnapshot& snapshot) noexcept {
    const bool is_enroll = Contains(snapshot.current_operation, kEnrollClient);
    const bool is_authentication = Contains(snapshot.current_operation, kAuthenticationClient);

    /**
     * 2026年08月27日23时20分
     * 状态优先取结构化字段，读不到时回退到 toString 文本匹配。
     * 部分系统版本没有该字段，只靠字段判断会让整套修复在这些版本上完全失效。
     * 2026 BY white
     */
    const bool state_started =
        (snapshot.has_operation_state && snapshot.operation_state == kStartedStateValue) ||
        Contains(snapshot.current_operation, kStartedStateText);

    const bool active = (is_enroll || is_authentication) && state_started;
    return HbmMonitorDecision{
        .current_is_enroll = is_enroll,
        .current_is_authentication = is_authentication,
        .state_started = state_started,
        .active_fingerprint_client = active,
        .should_start_hal = active,
    };
}

MonitorThreadStatus HbmMonitor::Start(JNIEnv* env, jobject preferred_loader,
                                      const HbmMonitorConfig& config) noexcept {
    if (!config.enabled || config.role != core::ProcessRole::kSystemServer) {
        return MonitorThreadStatus::kInvalidConfig;
    }
    if (config.scheduler_patch_interval <= 0) {
        return MonitorThreadStatus::kInvalidConfig;
    }

    auto task = std::unique_ptr<MonitorTask>(new (std::nothrow) HbmMonitorTask(config));
    if (task == nullptr) {
        return MonitorThreadStatus::kAllocationFailed;
    }

    return StartMonitorThread(env, preferred_loader,
                              MonitorThreadConfig{
                                  .log_tag = kLogTag,
                                  .max_attempts = config.max_attempts,
                                  .interval_ms = config.interval_ms,
                              },
                              std::move(task));
}

}  // namespace fod::monitor
