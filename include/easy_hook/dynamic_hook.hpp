#pragma once

#include <cstdint>

/**
 * 2026年08月27日22时42分
 * 运行时动态 hook 接口。三个声明式宏只能处理编译期就能写死的目标，
 * 而有些目标的地址要到运行期才拿得到，例如通过 JNI RegisterNatives 截获的原生方法指针、
 * 或从某个结构体字段里读出的回调地址。这类场景使用本接口。
 * 一般使用者不需要它，优先使用 EASY_HOOK_SYMBOL / EASY_HOOK_OFFSET / EASY_HOOK_PATTERN。
 * 2026 BY white
 */

namespace easy {

/**
 * 2026年08月27日22时42分
 * 在指定地址安装 inline hook，并把可调用原逻辑的跳板地址写入 original_slot。
 * 三个参数都不得为空，安装失败时不会修改 original_slot。
 * 2026 BY white
 */
[[nodiscard]] bool HookFunction(void* target, void* replacement, void** original_slot) noexcept;

/**
 * 2026年08月27日22时42分
 * 按模块名与符号名解析地址后安装 inline hook。模块名必须填写，
 * 留空会让底层符号解析器遍历并映射所有已加载模块，代价极高。
 * 2026 BY white
 */
[[nodiscard]] bool HookSymbol(const char* module_name, const char* symbol_name,
                              void* replacement, void** original_slot) noexcept;

/**
 * 2026年08月27日22时42分
 * 移除指定地址上的 hook 并还原原始指令。
 * 注意：若当前调用栈仍处在该函数内部，还原会改写正在执行的代码，
 * 此时必须推迟到调用返回之后再执行，否则会直接崩溃。
 * 2026 BY white
 */
[[nodiscard]] bool UnhookFunction(void* target) noexcept;

/**
 * 2026年08月27日22时42分
 * 解析模块中的符号地址，不安装任何 hook。可用于读取函数指针或校验目标是否存在。
 * 2026 BY white
 */
[[nodiscard]] void* ResolveSymbol(const char* module_name, const char* symbol_name) noexcept;

}  // namespace easy
