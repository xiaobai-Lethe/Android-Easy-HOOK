#pragma once

/**
 * 2026年08月27日21时46分
 * Easy Hook 框架对外总头文件。使用者只需包含本文件，即可用 EASY_HOOK_SYMBOL、
 * EASY_HOOK_OFFSET、EASY_HOOK_PATTERN 三个宏声明 hook，无需编写注册与初始化代码。
 * 同时导出注册表与运行时接口，供需要主动查询安装状态的高级用法使用。
 * 2026 BY white
 */

#include "easy_hook/core/install_state.hpp"
#include "easy_hook/core/log.hpp"
#include "easy_hook/core/registry.hpp"
#include "easy_hook/core/runtime.hpp"
#include "easy_hook/core/target.hpp"
#include "easy_hook/macros.hpp"

namespace easy {

/**
 * 2026年08月27日21时46分
 * 立即对所有尚未进入终态的 hook 执行一轮安装尝试，并返回本轮统计结果。
 * 框架已在库加载时自动调用一次，本接口仅供需要额外触发的场合使用。
 * 2026 BY white
 */
[[nodiscard]] inline core::InstallSummary InstallAll() noexcept {
    return core::Registry::Instance().InstallAll();
}

/**
 * 2026年08月27日21时46分
 * 把每个 hook 的目标、安装状态与解析出的绝对地址打印到日志，便于与 IDA 核对地址。
 * 2026 BY white
 */
inline void DumpStatus() noexcept {
    core::Registry::Instance().DumpStatus();
}

/**
 * 2026年08月27日21时46分
 * 返回仍在等待目标模块加载的 hook 数量，为零说明没有待补装的目标。
 * 2026 BY white
 */
[[nodiscard]] inline std::size_t PendingCount() noexcept {
    return core::Registry::Instance().pending_count();
}

}  // namespace easy
