#include "fod/monitor/cleanup_watchdog.hpp"

#include "fod/core/debug.hpp"
#include "fod/fingerprint/service_access.hpp"
#include "fod/jni/reflect.hpp"
#include "fod/udfps/scheduler_patcher.hpp"

#include <array>
#include <memory>
#include <new>
#include <string_view>

namespace fod::monitor {
namespace {

constexpr std::string_view kLogTag = "cleanup-watchdog";
constexpr std::string_view kCleanupClient = "FingerprintInternalCleanupClient";
constexpr std::string_view kEmptyDequeText = "[]";
constexpr std::string_view kSettingsOwner = "com.android.settings";
constexpr char kCancelMethod[] = "cancel";
constexpr char kCancelSignature[] =
    "(Landroid/os/Handler;Lcom/android/server/biometrics/sensors/ClientMonitorCallback;)V";

/**
 * 2026年08月27日23时26分
 * 被清理任务挡住时可能排队的指纹任务类型。命中其中任何一个都说明清理确实造成了阻塞。
 * 2026 BY white
 */
constexpr std::array<std::string_view, 3> kBlockedClients{
    "FingerprintGenerateChallengeClient",
    "FingerprintEnrollClient",
    "FingerprintRevokeChallengeClient",
};

bool Contains(std::string_view text, std::string_view value) noexcept {
    return !value.empty() && text.find(value) != std::string_view::npos;
}

/**
 * 2026年08月27日23时26分
 * 清理看门狗任务。确认清理操作确实阻塞后请求取消；
 * 取消成功即结束监控，其余失败都保留到下一轮重试。
 * 2026 BY white
 */
class CleanupWatchdogTask final : public MonitorTask {
public:
    explicit CleanupWatchdogTask(const CleanupWatchdogConfig& config) noexcept
        : config_(config) {}

    [[nodiscard]] bool Tick(JNIEnv* env, jobject preferred_loader, int attempt) noexcept override {
        static_cast<void>(attempt);

        fingerprint::ServiceAccess access(env, preferred_loader);
        jni::LocalRefScope scope(env);

        jobject service = scope.Track(access.ResolveService());
        if (service == nullptr) {
            return true;
        }

        /**
         * 2026年08月27日23时26分
         * 顺带重新应用传感器补丁。清理看门狗在开机早期反复运行，
         * 正好覆盖系统尚未稳定、字段可能被重新赋值的时间窗口。
         * 2026 BY white
         */
        static_cast<void>(
            udfps::SchedulerPatcher::PatchJavaService(env, service, config_.sensor_patch));

        jobject scheduler = scope.Track(access.ResolveScheduler(service));
        if (scheduler == nullptr) {
            return true;
        }
        jobject operation = scope.Track(access.ResolveCurrentOperation(scheduler));
        if (operation == nullptr) {
            return true;
        }

        const jni::ObjectDescription current = access.Describe(operation);
        if (current.status != jni::ObjectDescriptionStatus::kDescribed) {
            return true;
        }

        jobject pending = scope.Track(access.ResolvePendingOperations(scheduler));
        const jni::ObjectDescription pending_description = access.Describe(pending);
        if (pending_description.status == jni::ObjectDescriptionStatus::kFailed) {
            return true;
        }

        const CleanupWatchdogDecision decision = CleanupWatchdog::Evaluate({
            .current_operation = current.text,
            .pending_operations = pending_description.status ==
                                          jni::ObjectDescriptionStatus::kNull
                                      ? std::string_view{}
                                      : std::string_view(pending_description.text),
        });
        if (!decision.should_cancel) {
            return true;
        }

        jobject handler = scope.Track(access.ResolveHandler(scheduler));
        jobject callback = scope.Track(access.ResolveInternalCallback(scheduler));
        if (handler == nullptr || callback == nullptr) {
            return true;
        }

        if (!InvokeCancel(env, operation, handler, callback)) {
            return true;
        }

        core::Debug::Log(core::LogLevel::kInfo, kLogTag,
                         "cancelled stuck cleanup: pending_empty=%d fingerprint_pending=%d",
                         decision.pending_is_empty ? 1 : 0,
                         decision.has_fingerprint_pending ? 1 : 0);
        return false;
    }

private:
    /**
     * 2026年08月27日23时26分
     * 调用 BiometricSchedulerOperation.cancel(Handler, ClientMonitorCallback)。
     * 必须传入调度器自己的 handler 与内部回调，否则框架无法正确推进到下一个任务。
     * 2026 BY white
     */
    [[nodiscard]] static bool InvokeCancel(JNIEnv* env, jobject operation, jobject handler,
                                           jobject callback) noexcept {
#if defined(__ANDROID__)
        jclass operation_class = env->GetObjectClass(operation);
        if (jni::ConsumeException(env) || operation_class == nullptr) {
            return false;
        }
        const jmethodID cancel = env->GetMethodID(operation_class, kCancelMethod, kCancelSignature);
        jni::ConsumeException(env);
        env->DeleteLocalRef(operation_class);
        if (cancel == nullptr) {
            return false;
        }

        env->CallVoidMethod(operation, cancel, handler, callback);
        return !jni::ConsumeException(env);
#else
        static_cast<void>(env);
        static_cast<void>(operation);
        static_cast<void>(handler);
        static_cast<void>(callback);
        return false;
#endif
    }

    CleanupWatchdogConfig config_;
};

}  // namespace

CleanupWatchdogDecision CleanupWatchdog::Evaluate(
    const CleanupWatchdogSnapshot& snapshot) noexcept {
    CleanupWatchdogDecision decision{
        .current_is_cleanup = Contains(snapshot.current_operation, kCleanupClient),
        .pending_is_empty =
            snapshot.pending_operations.empty() ||
            Contains(snapshot.pending_operations, kEmptyDequeText),
        .has_settings_pending = Contains(snapshot.pending_operations, kSettingsOwner),
    };
    for (const std::string_view client : kBlockedClients) {
        decision.has_fingerprint_pending |= Contains(snapshot.pending_operations, client);
    }

    /**
     * 2026年08月27日23时26分
     * 队列为空同样要取消：此时清理任务已经卡死，虽然暂时没有排队者，
     * 但任何新来的指纹操作都会被它挡住。
     * 2026 BY white
     */
    decision.should_cancel =
        decision.current_is_cleanup &&
        (decision.pending_is_empty || decision.has_fingerprint_pending);
    return decision;
}

MonitorThreadStatus CleanupWatchdog::Start(JNIEnv* env, jobject preferred_loader,
                                           const CleanupWatchdogConfig& config) noexcept {
    if (!config.enabled || config.role != core::ProcessRole::kSystemServer) {
        return MonitorThreadStatus::kInvalidConfig;
    }
    if (config.max_attempts <= 0) {
        return MonitorThreadStatus::kInvalidConfig;
    }

    auto task = std::unique_ptr<MonitorTask>(new (std::nothrow) CleanupWatchdogTask(config));
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
