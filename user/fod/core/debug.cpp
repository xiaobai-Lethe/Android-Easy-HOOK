#include "fod/core/debug.hpp"

#include "easy_hook/core/log.hpp"

#include <cstdarg>
#include <cstdio>

namespace fod::core {
namespace {

constexpr std::size_t kMessageCapacity = 1024;

/**
 * 2026年08月27日22时52分
 * 把指纹模块的日志级别映射到框架日志级别。
 * 框架没有单独的 Verbose 级，Verbose 归入 Debug，二者同样只在开启详细日志时输出。
 * 2026 BY white
 */
easy::core::LogLevel ToFrameworkLevel(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::kVerbose:
        case LogLevel::kDebug:
            return easy::core::LogLevel::kDebug;
        case LogLevel::kInfo:
            return easy::core::LogLevel::kInfo;
        case LogLevel::kWarning:
            return easy::core::LogLevel::kWarning;
        case LogLevel::kError:
            return easy::core::LogLevel::kError;
    }
    return easy::core::LogLevel::kInfo;
}

}  // namespace

void Debug::InitializeFromProperties() noexcept {
    easy::core::Log::InitializeFromProperties();
}

void Debug::SetEnabled(bool enabled) noexcept {
    easy::core::Log::SetEnabled(enabled);
}

void Debug::SetVerbose(bool enabled) noexcept {
    easy::core::Log::SetVerbose(enabled);
}

bool Debug::IsEnabled() noexcept {
    return easy::core::Log::IsEnabled();
}

bool Debug::IsVerbose() noexcept {
    return easy::core::Log::IsVerbose();
}

bool Debug::ShouldLog(LogLevel level) noexcept {
    return easy::core::Log::ShouldLog(ToFrameworkLevel(level));
}

void Debug::Log(LogLevel level, std::string_view tag, const char* format, ...) noexcept {
    if (format == nullptr || !ShouldLog(level)) {
        return;
    }

    /**
     * 2026年08月27日22时52分
     * 框架日志入口是可变参数函数，无法直接转发 va_list，
     * 因此先在本层格式化成字符串，再以单个字符串参数交给框架输出。
     * 2026 BY white
     */
    char message[kMessageCapacity]{};
    std::va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    easy::core::Log::Print(ToFrameworkLevel(level), tag, "%s", message);
}

}  // namespace fod::core
