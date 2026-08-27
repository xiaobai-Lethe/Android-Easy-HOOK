#include "easy_hook/core/log.hpp"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

#if defined(__ANDROID__)
#include <android/log.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/system_properties.h>
#include <unistd.h>
#endif

#ifndef EASY_HOOK_DEBUG_DEFAULT
#define EASY_HOOK_DEBUG_DEFAULT 1
#endif

namespace easy::core {
namespace {

constexpr char kLogcatTag[] = "EASY-HOOK";
constexpr std::size_t kMessageCapacity = 2048;

std::atomic_bool debug_enabled{EASY_HOOK_DEBUG_DEFAULT != 0};
std::atomic_bool verbose_enabled{false};

#if defined(__ANDROID__)
constexpr char kCacheLogPath[] = "/data/local/tmp/easy_hook.log";
std::mutex cache_log_mutex;

/**
 * 2026年08月27日21时48分
 * 读取布尔型系统属性，属性缺失时返回调用方给出的回退值。
 * 2026 BY white
 */
bool ReadBooleanProperty(const char* name, bool fallback) noexcept {
    char value[PROP_VALUE_MAX]{};
    if (__system_property_get(name, value) <= 0) {
        return fallback;
    }
    return value[0] == '1' || value[0] == 'y' || value[0] == 'Y' || value[0] == 't' ||
           value[0] == 'T';
}

int ToAndroidPriority(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::kDebug:
            return ANDROID_LOG_DEBUG;
        case LogLevel::kInfo:
            return ANDROID_LOG_INFO;
        case LogLevel::kWarning:
            return ANDROID_LOG_WARN;
        case LogLevel::kError:
            return ANDROID_LOG_ERROR;
    }
    return ANDROID_LOG_DEFAULT;
}

/**
 * 2026年08月27日21时48分
 * 格式化当前墙上时钟时间，失败时写入占位串以保证输出行结构稳定。
 * 2026 BY white
 */
void FormatTimestamp(char* buffer, std::size_t capacity) noexcept {
    if (buffer == nullptr || capacity == 0) {
        return;
    }

    const std::time_t now = std::time(nullptr);
    std::tm local_time{};
    if (localtime_r(&now, &local_time) == nullptr ||
        std::strftime(buffer, capacity, "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        std::snprintf(buffer, capacity, "0000-00-00 00:00:00");
    }
}

/**
 * 2026年08月27日22时36分
 * 以裸系统调用打开缓存日志文件。
 * 这里刻意绕开 libc 的 open 包装：使用者完全可能 hook 掉 open、openat 之类的常用函数，
 * 若框架自身的日志再经由被 hook 的包装函数，就会在用户 hook 与框架日志之间形成同线程重入，
 * 曾导致 zygote 在启动阶段死锁而整机卡在开机动画。直接走内核可从根本上断开这条回路。
 * 2026 BY white
 */
int RawOpenAppend(const char* path) noexcept {
    return static_cast<int>(syscall(SYS_openat, AT_FDCWD, path,
                                    O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0640));
}

ssize_t RawWrite(int descriptor, const void* buffer, std::size_t length) noexcept {
    return static_cast<ssize_t>(syscall(SYS_write, descriptor, buffer, length));
}

void RawClose(int descriptor) noexcept {
    static_cast<void>(syscall(SYS_close, descriptor));
}

/**
 * 2026年08月27日21时48分
 * 把一条日志追加写入缓存文件。文件不可写时静默放弃，不影响 logcat 输出。
 * 2026 BY white
 */
void AppendCacheLog(std::string_view tag, const char* message) noexcept {
    if (!debug_enabled.load(std::memory_order_relaxed)) {
        return;
    }

    const std::lock_guard lock(cache_log_mutex);
    const int descriptor = RawOpenAppend(kCacheLogPath);
    if (descriptor < 0) {
        return;
    }

    char timestamp[32]{};
    FormatTimestamp(timestamp, sizeof(timestamp));

    char line[kMessageCapacity + 128]{};
    const int length = std::snprintf(line, sizeof(line), "%s [%.*s] %s\n", timestamp,
                                     static_cast<int>(tag.size()), tag.data(), message);
    if (length > 0) {
        const auto bounded = static_cast<std::size_t>(length) < sizeof(line)
                                 ? static_cast<std::size_t>(length)
                                 : sizeof(line) - 1;
        const ssize_t written = RawWrite(descriptor, line, bounded);
        static_cast<void>(written);
    }
    RawClose(descriptor);
}
#endif

}  // namespace

void Log::InitializeFromProperties() noexcept {
#if defined(__ANDROID__)
    const std::string debug_property(kDebugProperty);
    const std::string verbose_property(kVerboseProperty);
    SetEnabled(ReadBooleanProperty(debug_property.c_str(), IsEnabled()));
    SetVerbose(ReadBooleanProperty(verbose_property.c_str(), false));
#endif
}

void Log::SetEnabled(bool enabled) noexcept {
    debug_enabled.store(enabled, std::memory_order_relaxed);
}

void Log::SetVerbose(bool enabled) noexcept {
    verbose_enabled.store(enabled, std::memory_order_relaxed);
}

bool Log::IsEnabled() noexcept {
    return debug_enabled.load(std::memory_order_relaxed);
}

bool Log::IsVerbose() noexcept {
    return IsEnabled() && verbose_enabled.load(std::memory_order_relaxed);
}

bool Log::ShouldLog(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::kWarning:
        case LogLevel::kError:
            return true;
        case LogLevel::kDebug:
            return IsVerbose();
        case LogLevel::kInfo:
            return IsEnabled();
    }
    return false;
}

void Log::Print(LogLevel level, std::string_view tag, const char* format, ...) noexcept {
    if (format == nullptr || !ShouldLog(level)) {
        return;
    }

    /**
     * 2026年08月27日22时36分
     * 线程本地重入保护。使用者可能 hook 掉日志实现内部会用到的函数（例如 libc 的 open），
     * 那样一次日志输出会再次进入替换函数并回到这里，形成同线程无限重入：
     * 轻则栈溢出，重则在日志互斥量上自锁。曾导致 zygote 启动阶段整机卡死。
     * 这里让内层调用直接返回，外层照常完成输出，从而彻底断开回路。
     * 2026 BY white
     */
    static thread_local bool printing = false;
    if (printing) {
        return;
    }

    class ReentryGuard final {
    public:
        explicit ReentryGuard(bool& flag) noexcept : flag_(flag) { flag_ = true; }
        ~ReentryGuard() { flag_ = false; }

        ReentryGuard(const ReentryGuard&) = delete;
        ReentryGuard& operator=(const ReentryGuard&) = delete;

    private:
        bool& flag_;
    };
    const ReentryGuard guard(printing);

    char message[kMessageCapacity]{};
    std::va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

#if defined(__ANDROID__)
    __android_log_print(ToAndroidPriority(level), kLogcatTag, "[%.*s] %s",
                        static_cast<int>(tag.size()), tag.data(), message);
    AppendCacheLog(tag, message);
#else
    std::fprintf(stderr, "[%s] [%.*s] %s\n", kLogcatTag, static_cast<int>(tag.size()),
                 tag.data(), message);
#endif
}

}  // namespace easy::core
