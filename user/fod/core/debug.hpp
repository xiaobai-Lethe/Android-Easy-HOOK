#pragma once

#include <cstdint>
#include <string_view>

namespace fod::core {

/**
 * 2026年08月27日22时52分
 * 日志级别。保留移植前的五级定义，使各指纹模块的调用点无需改动。
 * kVerbose 会被映射到框架日志的 Debug 级。
 * 2026 BY white
 */
enum class LogLevel : std::uint8_t {
    kVerbose,
    kDebug,
    kInfo,
    kWarning,
    kError,
};

/**
 * 2026年08月27日22时52分
 * 指纹模块的日志出口。仅作为兼容层，实际输出全部委托给框架的 easy::core::Log，
 * 因此自动获得框架日志已有的线程本地重入保护与裸系统调用写文件能力：
 * 即使使用者 hook 掉了日志内部会用到的基础函数，也不会形成同线程重入而死锁。
 * 2026 BY white
 */
class Debug final {
public:
    Debug() = delete;

    static void InitializeFromProperties() noexcept;
    static void SetEnabled(bool enabled) noexcept;
    static void SetVerbose(bool enabled) noexcept;

    [[nodiscard]] static bool IsEnabled() noexcept;
    [[nodiscard]] static bool IsVerbose() noexcept;
    [[nodiscard]] static bool ShouldLog(LogLevel level) noexcept;

    static void Log(LogLevel level, std::string_view tag, const char* format, ...) noexcept
        __attribute__((format(printf, 3, 4)));
};

}  // namespace fod::core
