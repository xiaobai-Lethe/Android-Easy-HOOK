#pragma once

#include "easy_hook/backend/inline_hook_backend.hpp"

namespace easy::backend {

/**
 * 2026年08月27日21时46分
 * 基于 Dobby 的 inline hook 后端。非 Android 平台下所有操作直接返回失败，
 * 使框架可以在宿主机上编译通过以便做静态检查，而不引入平台相关的条件编译到引擎层。
 * 2026 BY white
 */
class DobbyBackend final : public InlineHookBackend {
public:
    [[nodiscard]] bool Hook(std::uintptr_t target, void* replacement,
                            void** original_slot) noexcept override;
    [[nodiscard]] bool Unhook(std::uintptr_t target) noexcept override;
    [[nodiscard]] const char* name() const noexcept override;
};

}  // namespace easy::backend
