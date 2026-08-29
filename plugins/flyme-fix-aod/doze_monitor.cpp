#include "flyme-fix-aod/doze_monitor.hpp"

#include "flyme-fix-aod/doze_policy.hpp"
#include "easy_hook/core/log.hpp"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#if defined(__ANDROID__)
#include <cerrno>
#include <fcntl.h>
#include <pthread.h>
#include <sys/system_properties.h>
#include <time.h>
#include <unistd.h>
#endif

namespace flyme_aod {
namespace {

constexpr std::string_view kLogTag = "flyme-aod";

#if defined(__ANDROID__)

// AOD 判据来源（Flyme / SurfaceFlinger 提供）。
constexpr char kScreenStateProp[] = "debug.tracing.screen_state";
constexpr char kAodEnabledProp[] = "persist.sys.meizu.aod";

// 可配置项：未设置就用默认，方便不同机型改节点而不用改代码。
constexpr char kFixEnabledProp[] = "persist.easy_hook.aod_fix_enabled";
constexpr char kNodePathProp[] = "persist.easy_hook.aod_node";
constexpr char kOnValueProp[] = "persist.easy_hook.aod_on_value";
constexpr char kOffValueProp[] = "persist.easy_hook.aod_off_value";

constexpr char kDefaultNodePath[] = "/sys/class/mi_display/disp-DSI-0/doze_brightness";
constexpr char kDefaultOnValue[] = "1";
constexpr char kDefaultOffValue[] = "0";

// 稳定 doze 时的低频补写间隔（秒），同时作为点亮态 wait 的超时。值不变时内核会 skip，不引发切换。
constexpr int kRefreshSeconds = 10;

// 进入 doze 后先等一小段再写，让电源模式切换（OFF/DOZE/ON 之间）稳定，避免写在切换瞬间闪紫屏。
constexpr int kDozeSettleMs = 500;

enum class MonitorPhase : std::uint8_t { kIdle, kStarting, kStarted };
std::atomic<MonitorPhase> g_phase{MonitorPhase::kIdle};

struct DozeConfig final {
    std::string node_path;
    std::string on_value;
    std::string off_value;
    bool enabled{true};
};

std::string GetProp(const char* name, const char* fallback) noexcept {
    char buffer[PROP_VALUE_MAX]{};
    const int length = __system_property_get(name, buffer);
    if (length <= 0) {
        return std::string(fallback);
    }
    return std::string(buffer, static_cast<std::size_t>(length));
}

int GetIntProp(const char* name, int fallback) noexcept {
    char buffer[PROP_VALUE_MAX]{};
    const int length = __system_property_get(name, buffer);
    if (length <= 0) {
        return fallback;
    }
    char* end = nullptr;
    const long value = std::strtol(buffer, &end, 10);
    if (end == buffer || end == nullptr || *end != '\0') {
        return fallback;
    }
    return static_cast<int>(value);
}

DozeConfig ReadConfig() noexcept {
    DozeConfig config;
    config.enabled = GetIntProp(kFixEnabledProp, 1) != 0;
    config.node_path = GetProp(kNodePathProp, kDefaultNodePath);
    config.on_value = GetProp(kOnValueProp, kDefaultOnValue);
    config.off_value = GetProp(kOffValueProp, kDefaultOffValue);
    return config;
}

bool WriteNode(const std::string& path, const std::string& value) noexcept {
    const int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        easy::core::Log::Print(easy::core::LogLevel::kWarning, kLogTag,
                               "open node failed path=%s errno=%d", path.c_str(), errno);
        return false;
    }

    const ssize_t written = write(fd, value.data(), value.size());
    const int saved_errno = errno;
    const int close_result = close(fd);
    const bool ok = written == static_cast<ssize_t>(value.size()) && close_result == 0;
    if (!ok) {
        easy::core::Log::Print(easy::core::LogLevel::kWarning, kLogTag,
                               "write node failed path=%s ret=%d errno=%d", path.c_str(),
                               static_cast<int>(written), saved_errno);
    }
    return ok;
}

void SleepMillis(int milliseconds) noexcept {
    if (milliseconds <= 0) {
        return;
    }
    timespec remaining{
        .tv_sec = milliseconds / 1000,
        .tv_nsec = static_cast<long>(milliseconds % 1000) * 1000000L,
    };
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
}

DozePolicyInput ReadPolicyInput() noexcept {
    return DozePolicyInput{
        .screen_state = GetIntProp(kScreenStateProp, 0),
        .aod_enabled = GetIntProp(kAodEnabledProp, 0) != 0,
    };
}

void* RunDozeMonitor(void*) noexcept {
    const DozeConfig config = ReadConfig();
    easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag,
                           "monitor started node=%s on=%s off=%s", config.node_path.c_str(),
                           config.on_value.c_str(), config.off_value.c_str());

    // 事件驱动：阻塞等 screen_state 变化。未点亮时无限阻塞（静止 0 轮询），
    // 只有点亮态才带超时用于低频补写。
    const prop_info* screen_pi = __system_property_find(kScreenStateProp);
    std::uint32_t serial = 0;
    bool armed = false;  // 是否已在稳定 doze 上写过点亮

    while (true) {
        if (screen_pi == nullptr) {
            screen_pi = __system_property_find(kScreenStateProp);
        }

        const bool should_light = DozePolicy::Evaluate(ReadPolicyInput()).should_light;

        if (should_light && !armed) {
            // 刚进 doze：先等电源模式切换稳定，再确认仍在 doze 才写，避免写在切换瞬间闪屏。
            SleepMillis(kDozeSettleMs);
            const DozePolicyInput settled = ReadPolicyInput();
            if (!DozePolicy::Evaluate(settled).should_light) {
                continue;  // 这一小段里又离开 doze（如立刻亮屏），放弃本次点亮
            }
            const bool ok = WriteNode(config.node_path, config.on_value);
            armed = true;
            easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag,
                                   "armed screen_state=%d aod=%d ok=%d", settled.screen_state,
                                   settled.aod_enabled ? 1 : 0, ok ? 1 : 0);
        } else if (should_light && armed) {
            // 稳定 doze 的低频补写：值不变时内核会 skip，不引发切换/闪屏。
            static_cast<void>(WriteNode(config.node_path, config.on_value));
        } else if (!should_light && armed) {
            // 离开 doze：不主动写 off，系统自己会做 doze->normal / nolp，主动写反而可能撞车闪屏。
            armed = false;
            easy::core::Log::Print(easy::core::LogLevel::kInfo, kLogTag, "disarmed");
        }

        if (screen_pi == nullptr) {
            SleepMillis(kRefreshSeconds * 1000);
            continue;
        }

        std::uint32_t new_serial = serial;
        if (armed) {
            const timespec timeout{.tv_sec = kRefreshSeconds, .tv_nsec = 0};
            static_cast<void>(__system_property_wait(screen_pi, serial, &new_serial, &timeout));
        } else {
            // 未点亮：无限阻塞，只有屏幕状态真正变化才醒来，静止时 0 轮询。
            static_cast<void>(__system_property_wait(screen_pi, serial, &new_serial, nullptr));
        }
        serial = new_serial;
    }
    return nullptr;
}

#endif  // __ANDROID__

}  // namespace

const char* DozeMonitorStatusName(DozeMonitorStatus status) noexcept {
    switch (status) {
        case DozeMonitorStatus::kStarted:
            return "started";
        case DozeMonitorStatus::kAlreadyStarted:
            return "already_started";
        case DozeMonitorStatus::kDisabled:
            return "disabled";
        case DozeMonitorStatus::kThreadStartFailed:
            return "thread_start_failed";
        case DozeMonitorStatus::kUnsupportedPlatform:
            return "unsupported_platform";
    }
    return "invalid";
}

DozeMonitorStatus DozeMonitor::Start() noexcept {
#if defined(__ANDROID__)
    if (GetIntProp(kFixEnabledProp, 1) == 0) {
        return DozeMonitorStatus::kDisabled;
    }

    MonitorPhase expected = MonitorPhase::kIdle;
    if (!g_phase.compare_exchange_strong(expected, MonitorPhase::kStarting,
                                         std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
        return DozeMonitorStatus::kAlreadyStarted;
    }

    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) {
        g_phase.store(MonitorPhase::kIdle, std::memory_order_release);
        return DozeMonitorStatus::kThreadStartFailed;
    }
    // 与宿主进程同生命周期、无人 join，必须建为分离线程。
    if (pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED) != 0) {
        pthread_attr_destroy(&attributes);
        g_phase.store(MonitorPhase::kIdle, std::memory_order_release);
        return DozeMonitorStatus::kThreadStartFailed;
    }

    pthread_t thread{};
    const int create_result = pthread_create(&thread, &attributes, RunDozeMonitor, nullptr);
    pthread_attr_destroy(&attributes);
    if (create_result != 0) {
        g_phase.store(MonitorPhase::kIdle, std::memory_order_release);
        return DozeMonitorStatus::kThreadStartFailed;
    }

    g_phase.store(MonitorPhase::kStarted, std::memory_order_release);
    return DozeMonitorStatus::kStarted;
#else
    return DozeMonitorStatus::kUnsupportedPlatform;
#endif
}

}  // namespace flyme_aod
