#pragma once

#include "easy_hook/locator/target_locator.hpp"

namespace easy::locator {

/**
 * 2026年08月27日21时46分
 * 按模块名加相对虚拟地址定位。偏移取 IDA 中显示的地址，内部用模块的 load_bias 换算成
 * 运行时绝对地址，并校验该地址确实落在某个已映射段内，以拦截写错的偏移。
 * 2026 BY white
 */
class OffsetLocator final : public TargetLocator {
public:
    [[nodiscard]] LocateResult Locate(const core::TargetSpec& spec) noexcept override;
};

}  // namespace easy::locator
