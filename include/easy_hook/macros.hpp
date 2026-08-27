#pragma once

#include "easy_hook/core/log.hpp"
#include "easy_hook/core/process_filter.hpp"
#include "easy_hook/core/registration.hpp"
#include "easy_hook/core/target.hpp"

/**
 * 2026年08月27日21时46分
 * 本文件定义用户唯一需要接触的宏。每个 hook 宏把替换函数、原函数指针和自注册对象
 * 一起包进一个以 hook 名命名的独立结构体，因此 EASY_ORIGINAL 能在函数体内直接可见，
 * 又不会向全局命名空间引入任何符号，多个 hook 之间也不会相互干扰。
 * 2026 BY white
 */

#define EASY_HOOK_INTERNAL_DEFINE(hook_name, spec_expression, return_type, ...)         \
    namespace easy_user_hooks {                                                        \
    struct hook_name final {                                                           \
        static return_type Replacement(__VA_ARGS__);                                    \
        static inline return_type (*original)(__VA_ARGS__) = nullptr;                   \
        [[maybe_unused]] static inline ::easy::core::Registration kRegistration{        \
            (spec_expression), reinterpret_cast<void*>(&Replacement),                    \
            reinterpret_cast<void**>(&original)};                                        \
    };                                                                                 \
    }                                                                                  \
    return_type easy_user_hooks::hook_name::Replacement(__VA_ARGS__)

/**
 * 2026年08月27日21时46分
 * 按模块名加符号名声明一个 hook。符号可以是未导出的内部符号。
 * 用法：EASY_HOOK_SYMBOL(名字, "libc.so", "open", int, const char* path, int flags) { ... }
 * 2026 BY white
 */
#define EASY_HOOK_SYMBOL(hook_name, module_name, symbol_name, return_type, ...) \
    EASY_HOOK_INTERNAL_DEFINE(                                                  \
        hook_name,                                                              \
        ::easy::core::TargetSpec::Symbol(#hook_name, module_name, symbol_name),  \
        return_type __VA_OPT__(, ) __VA_ARGS__)

/**
 * 2026年08月27日21时46分
 * 按模块名加相对虚拟地址声明一个 hook，偏移直接填 IDA 中显示的地址。
 * 用法：EASY_HOOK_OFFSET(名字, "libgame.so", 0x1A2B0, int, void* self, int raw) { ... }
 * 2026 BY white
 */
#define EASY_HOOK_OFFSET(hook_name, module_name, module_offset, return_type, ...) \
    EASY_HOOK_INTERNAL_DEFINE(                                                   \
        hook_name,                                                               \
        ::easy::core::TargetSpec::Offset(                                        \
            #hook_name, module_name,                                             \
            static_cast<::std::uintptr_t>(module_offset)),                        \
        return_type __VA_OPT__(, ) __VA_ARGS__)

/**
 * 2026年08月27日21时46分
 * 按字节特征码声明一个 hook，?? 表示任意字节。特征码必须在目标模块可执行段中唯一命中。
 * 用法：EASY_HOOK_PATTERN(名字, "libgame.so", "1F 20 ?? D5", int, void* buf, int len) { ... }
 * 2026 BY white
 */
#define EASY_HOOK_PATTERN(hook_name, module_name, byte_pattern, return_type, ...) \
    EASY_HOOK_INTERNAL_DEFINE(                                                    \
        hook_name,                                                                \
        ::easy::core::TargetSpec::Pattern(#hook_name, module_name, byte_pattern),  \
        return_type __VA_OPT__(, ) __VA_ARGS__)

/**
 * 2026年08月27日21时46分
 * 在替换函数体内调用被 hook 的原逻辑。参数与声明时的参数列表一致。
 * 不调用即表示完全接管该函数。
 * 2026 BY white
 */
#define EASY_ORIGINAL(...) original(__VA_ARGS__)

/**
 * 2026年08月27日21时46分
 * 判断原函数是否已成功接管，用于替换函数中需要区分"未安装成功"的场合。
 * 2026 BY white
 */
#define EASY_HAS_ORIGINAL() (original != nullptr)

/**
 * 2026年08月27日21时46分
 * 在替换函数体或任意用户代码中输出一条日志，标签固定为 user。
 * 2026 BY white
 */
#define EASY_LOG(...) \
    ::easy::core::Log::Print(::easy::core::LogLevel::kInfo, "user", __VA_ARGS__)

/**
 * 2026年08月27日21时46分
 * 输出一条错误级日志，不受调试开关影响，始终写入。
 * 2026 BY white
 */
#define EASY_LOG_ERROR(...) \
    ::easy::core::Log::Print(::easy::core::LogLevel::kError, "user", __VA_ARGS__)

#define EASY_HOOK_INTERNAL_CONCAT_INNER(left, right) left##right
#define EASY_HOOK_INTERNAL_CONCAT(left, right) EASY_HOOK_INTERNAL_CONCAT_INNER(left, right)

/**
 * 2026年08月27日21时46分
 * 限定本框架只在指定进程中生效，可在多个位置声明以允许多个进程。
 * 完全不声明时不做过滤，注入到哪个进程就在哪个进程生效。
 * 用法：EASY_HOOK_ONLY_IN("com.example.game")
 * 2026 BY white
 */
#define EASY_HOOK_ONLY_IN(process_name)                                     \
    namespace easy_user_hooks {                                             \
    static const ::easy::core::ProcessFilter EASY_HOOK_INTERNAL_CONCAT(     \
        kProcessFilter_, __LINE__){process_name};                           \
    }
