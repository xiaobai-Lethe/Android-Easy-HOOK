#pragma once

#include "easy_hook/core/registry.hpp"

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * 框架启动结果，供入口日志说明本进程实际做了什么。
 * kNoRegistration 只表示启动那一刻注册表为空，此后登记的 hook 仍会被正常安装。
 * 2026 BY white
 */
enum class RuntimeStartStatus : unsigned char {
    kStarted,
    kAlreadyStarted,
    kSkippedProcessFilter,
    kNoRegistration,
};

/**
 * 2026年08月27日21时46分
 * 框架运行时编排。库被加载时由构造函数调用一次，完成日志初始化、进程过滤判定、
 * 模块加载监听安装和首轮 hook 安装。多次调用只有首次生效。
 * 2026 BY white
 */
class Runtime final {
public:
    Runtime() = delete;

    /**
     * 2026年08月27日21时46分
     * 启动框架。目标模块尚未加载的 hook 会留在挂起状态，由模块加载监听后续补装。
     * 2026 BY white
     */
    static RuntimeStartStatus Start() noexcept;

    [[nodiscard]] static const char* StartStatusName(RuntimeStartStatus status) noexcept;
};

}  // namespace easy::core
