#pragma once

#include "easy_hook/backend/inline_hook_backend.hpp"
#include "easy_hook/core/registration.hpp"
#include "easy_hook/locator/offset_locator.hpp"
#include "easy_hook/locator/pattern_locator.hpp"
#include "easy_hook/locator/symbol_locator.hpp"

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * hook 安装引擎。负责按定位方式选择定位器解析目标地址，交由 inline hook 后端安装，
 * 并把结果与解析出的绝对地址写回注册项、输出安装日志。
 * 引擎自身不加锁，调用方（注册表）负责在持有锁的前提下串行调用。
 * 2026 BY white
 */
class HookEngine final {
public:
    /**
     * 2026年08月27日21时46分
     * 返回进程内默认引擎实例，绑定默认的 inline hook 后端。
     * 2026 BY white
     */
    [[nodiscard]] static HookEngine& Instance() noexcept;

    explicit HookEngine(backend::InlineHookBackend& hook_backend) noexcept;

    HookEngine(const HookEngine&) = delete;
    HookEngine& operator=(const HookEngine&) = delete;

    /**
     * 2026年08月27日21时46分
     * 对单个注册项执行一次安装尝试，结果写回注册项并返回是否安装成功。
     * 2026 BY white
     */
    bool Install(Registration& registration) noexcept;

private:
    /**
     * 2026年08月27日21时46分
     * 按定位方式取得对应的定位器，规格非法时返回空指针。
     * 2026 BY white
     */
    [[nodiscard]] locator::TargetLocator* LocatorFor(TargetKind kind) noexcept;

    backend::InlineHookBackend& backend_;
    locator::SymbolLocator symbol_locator_;
    locator::OffsetLocator offset_locator_;
    locator::PatternLocator pattern_locator_;
};

}  // namespace easy::core
