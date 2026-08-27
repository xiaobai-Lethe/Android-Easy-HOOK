#pragma once

#include "easy_hook/locator/target_locator.hpp"

namespace easy::locator {

/**
 * 2026年08月27日21时46分
 * 按字节特征码定位。在目标模块的可执行段中搜索，命中零次或多于一次都视为失败，
 * 后者说明特征码不够唯一，继续安装会在不同版本的目标库上 hook 到错误位置。
 * 2026 BY white
 */
class PatternLocator final : public TargetLocator {
public:
    [[nodiscard]] LocateResult Locate(const core::TargetSpec& spec) noexcept override;
};

}  // namespace easy::locator
