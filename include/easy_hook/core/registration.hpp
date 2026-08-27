#pragma once

#include "easy_hook/core/install_state.hpp"
#include "easy_hook/core/target.hpp"

#include <cstdint>

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * 一个 hook 的注册项。用户宏在每个 hook 处定义一个静态实例，构造期自动挂入全局注册表，
 * 因此用户无需编写任何注册代码。对象具有静态存储期，生命周期覆盖整个进程，
 * 注册表只保存裸指针而不接管所有权。
 * 2026 BY white
 */
class Registration final {
public:
    /**
     * 2026年08月27日21时46分
     * 构造并立即把自身登记到全局注册表。replacement 为替换函数地址，
     * original_slot 指向用户宏中的原函数指针变量，安装成功后由后端写入跳板地址。
     * 2026 BY white
     */
    Registration(const TargetSpec& spec, void* replacement, void** original_slot) noexcept;

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&&) = delete;
    Registration& operator=(Registration&&) = delete;
    ~Registration() = default;

    [[nodiscard]] const TargetSpec& spec() const noexcept { return spec_; }
    [[nodiscard]] void* replacement() const noexcept { return replacement_; }
    [[nodiscard]] void** original_slot() const noexcept { return original_slot_; }
    [[nodiscard]] InstallState state() const noexcept { return state_; }
    [[nodiscard]] std::uintptr_t resolved_address() const noexcept { return resolved_address_; }

    /**
     * 2026年08月27日21时46分
     * 由引擎在一次安装尝试结束后写回结果。仅引擎在持有注册表锁时调用，因此无需额外同步。
     * 2026 BY white
     */
    void SetResult(InstallState state, std::uintptr_t resolved_address) noexcept;

private:
    TargetSpec spec_;
    void* replacement_{nullptr};
    void** original_slot_{nullptr};
    InstallState state_{InstallState::kNotAttempted};
    std::uintptr_t resolved_address_{0};
};

}  // namespace easy::core
