#pragma once

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * 单个 hook 的安装结果。kModuleNotLoaded 表示目标模块尚未加载，
 * 会等待模块加载监听触发补装；其余失败状态均为终态，不再重试，
 * 避免在每次 dlopen 上反复解析同一个不存在的目标。
 * 2026 BY white
 */
enum class InstallState : unsigned char {
    kNotAttempted,
    kInstalled,
    kSkippedProcessFilter,
    kInvalidSpec,
    kModuleNotLoaded,
    kSymbolNotFound,
    kOffsetOutOfRange,
    kPatternInvalid,
    kPatternNotFound,
    kPatternAmbiguous,
    kBackendFailed,
};

/**
 * 2026年08月27日21时46分
 * 返回安装状态的可读名称，用于日志与状态汇总输出。
 * 2026 BY white
 */
[[nodiscard]] const char* InstallStateName(InstallState state) noexcept;

/**
 * 2026年08月27日21时46分
 * 判断该状态是否值得在新模块加载后重试。只有目标模块缺失属于暂时性失败。
 * 2026 BY white
 */
[[nodiscard]] bool IsRetryableState(InstallState state) noexcept;

}  // namespace easy::core
