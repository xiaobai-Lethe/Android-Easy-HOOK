#pragma once

#include <cstdint>

namespace easy::backend {

/**
 * 2026年08月27日21时46分
 * inline hook 后端抽象接口。引擎只依赖本接口，不直接引用任何具体 hook 库，
 * 二期接入 ART 方法 hook 时新增一个实现即可，无需改动引擎、注册表与定位层。
 * 2026 BY white
 */
class InlineHookBackend {
public:
    InlineHookBackend() = default;
    virtual ~InlineHookBackend() = default;

    InlineHookBackend(const InlineHookBackend&) = delete;
    InlineHookBackend& operator=(const InlineHookBackend&) = delete;

    /**
     * 2026年08月27日21时46分
     * 在 target 处安装跳转到 replacement 的 inline hook，并把可调用原逻辑的跳板地址
     * 写入 original_slot。三个参数均不得为空，安装失败时不修改 original_slot。
     * 2026 BY white
     */
    [[nodiscard]] virtual bool Hook(std::uintptr_t target, void* replacement,
                                    void** original_slot) noexcept = 0;

    /**
     * 2026年08月27日21时46分
     * 移除 target 处的 hook 并还原原始指令。
     * 2026 BY white
     */
    [[nodiscard]] virtual bool Unhook(std::uintptr_t target) noexcept = 0;

    /**
     * 2026年08月27日21时46分
     * 返回后端名称，用于安装日志中标明实际生效的 hook 实现。
     * 2026 BY white
     */
    [[nodiscard]] virtual const char* name() const noexcept = 0;
};

/**
 * 2026年08月27日21时46分
 * 返回进程内默认使用的 inline hook 后端实例。
 * 2026 BY white
 */
[[nodiscard]] InlineHookBackend& DefaultInlineHookBackend() noexcept;

}  // namespace easy::backend
