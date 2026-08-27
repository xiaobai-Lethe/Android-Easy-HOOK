#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace fod::core {

// kOther 是已确认的非目标进程；kUnknown 表示身份信息读取失败。
enum class ProcessRole : std::uint8_t {
    kUnknown,
    kOther,
    kZygote,
    kSystemServer,
    kSettings,
    kSystemUi,
};

struct ProcessSnapshot final {
    std::string command_line;
    std::string selinux_context;
    ProcessRole role{ProcessRole::kUnknown};
};

class ProcessContext final {
public:
    ProcessContext() = delete;

    // 同时检查进程名和 SELinux 上下文，避免仅凭可伪造的进程名作判断。
    [[nodiscard]] static ProcessRole Classify(std::string_view command_line,
                                              std::string_view selinux_context) noexcept;
    [[nodiscard]] static ProcessSnapshot ReadCurrent();

    /**
     * 2026年08月27日23时46分
     * 取当前进程角色，结果按进程缓存。
     *
     * 角色判定要读两个 /proc 文件，而 Binder 拦截会在每一次事务上询问角色，
     * 逐次读取会给系统里所有进程都加上可观的固定开销。
     * 缓存以 pid 作为有效性凭据：本框架经 zygote 注入，子进程会继承父进程的静态数据，
     * 若只用一个布尔标记，所有应用进程都会沿用 zygote 的判定结果而永远认不出自己的身份。
     * 2026 BY white
     */
    [[nodiscard]] static ProcessRole CurrentRole() noexcept;
};

}  // namespace fod::core
