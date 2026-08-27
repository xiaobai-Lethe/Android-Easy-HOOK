#include "fod/monitor/challenge_watchdog.hpp"

#include "easy_hook/core/log.hpp"
#include "fod/fingerprint/service_access.hpp"
#include "fod/jni/reflect.hpp"
#include "fod/udfps/scheduler_patcher.hpp"

#include <memory>
#include <new>
#include <string_view>

namespace fod::monitor {
namespace {

constexpr std::string_view kLogTag = "challenge-watchdog";
constexpr std::string_view kGenerateChallengeClient = "FingerprintGenerateChallengeClient";
constexpr std::string_view kSettingsOwner = "com.android.settings";
constexpr std::string_view kStartedStateText = "State: 2";
constexpr char kStartHalMethod[] = "startHalOperation";

bool Contains(std::string_view text, std::string_view value) noexcept {
    return !value.empty() && text.find(value) != std::string_view::npos;
}

/**
 * 2026年08月27日23时32分
 * 挑战看门狗任务。连续多轮确认卡住后重新启动一次 HAL 操作，
 * 重试成功即结束监控，整个进程生命周期内最多重试一次。
 * 2026 BY white
 */
class ChallengeWatchdogTask final : public MonitorTask {
public:
    explicit ChallengeWatchdogTask(const ChallengeWatchdogConfig& config) noexcept
        : config_(config) {}

    [[nodiscard]] bool Tick(JNIEnv* env, jobject preferred_loader, int attempt) noexcept override {
        static_cast<void>(attempt);

        fingerprint::ServiceAccess access(env, preferred_loader);
        jni::LocalRefScope scope(env);

        jobject service = scope.Track(access.ResolveService());
        if (service == nullptr) {
            return true;
        }
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

        const ChallengeWatchdogDecision decision =
            ChallengeWatchdog::Evaluate({.current_operation = current.text});

        /**
         * 2026年08月27日23时32分
         * 当前任务已经不是 generateChallenge，说明之前观察到的卡顿自行恢复了，
         * 连续命中计数必须清零，否则下一次真正卡住时会提前触发重试。
         * 2026 BY white
         */
        if (!decision.current_is_generate) {
            observed_stuck_count_ = 0;
            return true;
        }
        if (!decision.is_stuck_candidate) {
            return true;
        }

        ++observed_stuck_count_;
        if (observed_stuck_count_ < ChallengeWatchdog::kRequiredStableObservations) {
            return true;
        }

        jobject client = scope.Track(access.ResolveCurrentClient(operation));
        if (client == nullptr) {
            return true;
        }
        if (!InvokeStartHalOperation(env, client)) {
            return true;
        }

        easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag,
                               "restarted stuck generateChallenge after %d stable observations",
                               observed_stuck_count_);
        return false;
    }

private:
    [[nodiscard]] static bool InvokeStartHalOperation(JNIEnv* env, jobject client) noexcept {
#if defined(__ANDROID__)
        jclass client_class = env->GetObjectClass(client);
        if (jni::ConsumeException(env) || client_class == nullptr) {
            return false;
        }
        const jmethodID start_hal = env->GetMethodID(client_class, kStartHalMethod, "()V");
        jni::ConsumeException(env);
        env->DeleteLocalRef(client_class);
        if (start_hal == nullptr) {
            return false;
        }

        env->CallVoidMethod(client, start_hal);
        return !jni::ConsumeException(env);
#else
        static_cast<void>(env);
        static_cast<void>(client);
        return false;
#endif
    }

    ChallengeWatchdogConfig config_;
    int observed_stuck_count_{0};
};

}  // namespace

ChallengeWatchdogDecision ChallengeWatchdog::Evaluate(
    const ChallengeWatchdogSnapshot& snapshot) noexcept {
    ChallengeWatchdogDecision decision{
        .current_is_generate = Contains(snapshot.current_operation, kGenerateChallengeClient),
        .owner_is_settings = Contains(snapshot.current_operation, kSettingsOwner),
        .state_started = Contains(snapshot.current_operation, kStartedStateText),
    };
    decision.is_stuck_candidate =
        decision.current_is_generate && decision.owner_is_settings && decision.state_started;
    return decision;
}

MonitorThreadStatus ChallengeWatchdog::Start(JNIEnv* env, jobject preferred_loader,
                                             const ChallengeWatchdogConfig& config) noexcept {
    if (!config.enabled || config.role != core::ProcessRole::kSystemServer) {
        return MonitorThreadStatus::kInvalidConfig;
    }
    if (config.max_attempts <= 0) {
        return MonitorThreadStatus::kInvalidConfig;
    }

    auto task = std::unique_ptr<MonitorTask>(new (std::nothrow) ChallengeWatchdogTask(config));
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
