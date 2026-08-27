#pragma once

#include <string_view>

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * 日志级别。Warning 与 Error 始终输出，Debug 与 Info 受调试开关控制。
 * 2026 BY white
 */
enum class LogLevel : unsigned char {
    kDebug,
    kInfo,
    kWarning,
    kError,
};

/**
 * 2026年08月27日21时46分
 * 框架统一日志出口，负责读取系统属性开关、写入 logcat 并按需追加缓存日志文件。
 * 所有成员为静态函数，进程内共享同一份开关状态，可在任意线程安全调用。
 * 2026 BY white
 */
class Log final {
public:
    static constexpr std::string_view kDebugProperty = "persist.easy_hook.debug";
    static constexpr std::string_view kVerboseProperty = "persist.easy_hook.verbose";

    Log() = delete;

    /**
     * 2026年08月27日21时46分
     * 从系统属性刷新调试与详细日志开关，供构造函数入口在最早时机调用。
     * 2026 BY white
     */
    static void InitializeFromProperties() noexcept;

    static void SetEnabled(bool enabled) noexcept;
    static void SetVerbose(bool enabled) noexcept;
    [[nodiscard]] static bool IsEnabled() noexcept;
    [[nodiscard]] static bool IsVerbose() noexcept;
    [[nodiscard]] static bool ShouldLog(LogLevel level) noexcept;

    /**
     * 2026年08月27日21时46分
     * 按级别格式化输出一条日志。格式串为空或级别被过滤时直接返回，不抛出异常。
     * 2026 BY white
     */
    static void Print(LogLevel level, std::string_view tag, const char* format, ...) noexcept
        __attribute__((format(printf, 3, 4)));
};

}  // namespace easy::core
