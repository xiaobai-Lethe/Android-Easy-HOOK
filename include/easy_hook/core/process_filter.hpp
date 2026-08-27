#pragma once

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * 进程过滤声明器。用户宏 EASY_HOOK_ONLY_IN 在文件作用域定义一个静态实例，
 * 构造期把进程名追加到注册表的允许列表。可声明多次以允许多个进程。
 * 2026 BY white
 */
class ProcessFilter final {
public:
    explicit ProcessFilter(const char* process_name) noexcept;

    ProcessFilter(const ProcessFilter&) = delete;
    ProcessFilter& operator=(const ProcessFilter&) = delete;
};

/**
 * 2026年08月27日21时46分
 * 读取当前进程 /proc/self/cmdline 的首个字段作为进程名。读取失败时返回空串。
 * 2026 BY white
 */
[[nodiscard]] const char* CurrentProcessName() noexcept;

}  // namespace easy::core
