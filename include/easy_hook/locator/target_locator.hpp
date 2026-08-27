#pragma once

#include "easy_hook/core/install_state.hpp"
#include "easy_hook/core/target.hpp"

#include <cstdint>

namespace easy::locator {

/**
 * 2026年08月27日21时46分
 * 一次目标定位的结果。state 为 kInstalled 之外的值时 address 无意义；
 * 定位器只负责解析地址，是否可重试由 core::IsRetryableState 统一判定。
 * 2026 BY white
 */
struct LocateResult final {
    std::uintptr_t address{0};
    core::InstallState state{core::InstallState::kNotAttempted};

    [[nodiscard]] bool resolved() const noexcept { return address != 0; }

    [[nodiscard]] static LocateResult Failure(core::InstallState state) noexcept {
        return LocateResult{.address = 0, .state = state};
    }

    [[nodiscard]] static LocateResult Success(std::uintptr_t address) noexcept {
        return LocateResult{.address = address, .state = core::InstallState::kInstalled};
    }
};

/**
 * 2026年08月27日21时46分
 * 目标定位器抽象接口。引擎按 TargetKind 选择具体实现，新增定位方式只需实现本接口，
 * 无需改动引擎与注册表。
 * 2026 BY white
 */
class TargetLocator {
public:
    TargetLocator() = default;
    virtual ~TargetLocator() = default;

    TargetLocator(const TargetLocator&) = delete;
    TargetLocator& operator=(const TargetLocator&) = delete;

    [[nodiscard]] virtual LocateResult Locate(const core::TargetSpec& spec) noexcept = 0;
};

}  // namespace easy::locator
