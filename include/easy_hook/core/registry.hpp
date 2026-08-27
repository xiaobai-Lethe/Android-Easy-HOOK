#pragma once

#include "easy_hook/core/registration.hpp"

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * 一轮安装动作的统计结果，用于日志汇总和调用方判断是否还有待补装的目标。
 * 2026 BY white
 */
struct InstallSummary final {
    std::size_t total{0};
    std::size_t installed{0};
    std::size_t pending{0};
    std::size_t failed{0};
    std::size_t skipped{0};
};

/**
 * 2026年08月27日21时46分
 * 全局 hook 注册表。采用函数内静态单例，保证在任意翻译单元的静态构造期首次访问时才创建，
 * 从根本上避免跨翻译单元的静态初始化顺序问题。内部以互斥量保护，可被多线程访问。
 * 2026 BY white
 */
class Registry final {
public:
    /**
     * 2026年08月27日21时46分
     * 返回全局唯一实例。函数内静态对象的初始化由 C++ 运行时保证线程安全。
     * 2026 BY white
     */
    [[nodiscard]] static Registry& Instance() noexcept;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    /**
     * 2026年08月27日21时46分
     * 登记一个注册项。注册项由用户宏静态持有，这里只保存指针，不接管所有权。
     * 若运行时已经启动，则立即为该注册项执行一次安装，使 hook 的生效与否
     * 完全不依赖各翻译单元静态初始化的先后顺序。
     * 2026 BY white
     */
    void Add(Registration* registration) noexcept;

    /**
     * 2026年08月27日22时10分
     * 由运行时在通过进程过滤后开启，开启后新登记的注册项会被立即安装。
     * 这是框架不依赖 .init_array 顺序的关键：库入口构造函数与用户 hook 的静态初始化
     * 同处 .init_array，谁先执行取决于链接顺序，任何一方先执行都必须能正确工作。
     * 2026 BY white
     */
    void SetInstallActive(bool active) noexcept;

    [[nodiscard]] bool install_active() const noexcept;

    /**
     * 2026年08月27日21时46分
     * 追加一个允许生效的进程名。列表为空时表示不做进程过滤，注入到哪就在哪生效。
     * 2026 BY white
     */
    void AddAllowedProcess(const char* process_name) noexcept;

    /**
     * 2026年08月27日21时46分
     * 判断当前进程是否允许安装 hook。读取 /proc/self/cmdline 的首个字段与允许列表比对，
     * 支持精确匹配以及 "包名:子进程" 形式的前缀匹配。
     * 2026 BY white
     */
    [[nodiscard]] bool IsCurrentProcessAllowed() const noexcept;

    /**
     * 2026年08月27日21时46分
     * 对所有尚未进入终态的注册项执行一轮安装尝试，并返回本轮统计结果。
     * 2026 BY white
     */
    InstallSummary InstallAll() noexcept;

    /**
     * 2026年08月27日21时46分
     * 仅对处于可重试状态的注册项再次尝试安装，供模块加载监听在新模块就绪后调用。
     * 2026 BY white
     */
    InstallSummary RetryPending() noexcept;

    /**
     * 2026年08月27日21时46分
     * 把每个注册项的目标、状态和解析出的绝对地址打印到日志，便于与 IDA 核对。
     * 2026 BY white
     */
    void DumpStatus() const noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t pending_count() const noexcept;

private:
    Registry() = default;

    /**
     * 2026年08月27日22时38分
     * 执行一轮安装。先在锁内取注册项快照，随后在不持锁的情况下逐个安装。
     * 安装过程会解析符号并改写代码，其中可能触发模块加载，进而回调到本类；
     * 若全程持锁，这条回路会在同一线程上自锁。快照加不持锁安装可从结构上避免该问题。
     * 2026 BY white
     */
    InstallSummary RunInstall(bool retry_only) noexcept;

    mutable std::mutex mutex_;

    /**
     * 2026年08月27日22时38分
     * 仅用于串行化安装动作本身，与保护容器的 mutex_ 分开，避免安装期间阻塞状态查询。
     * 2026 BY white
     */
    std::mutex install_mutex_;

    std::vector<Registration*> registrations_;
    std::vector<std::string> allowed_processes_;
    bool install_active_{false};
};

}  // namespace easy::core
