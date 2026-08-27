#pragma once

#include "easy_hook/locator/target_locator.hpp"

namespace easy::locator {

/**
 * 2026年08月27日21时46分
 * 按模块名加符号名定位。底层使用 Dobby 的符号解析器，它先尝试 dlsym，
 * 失败后会把该模块文件映射进来解析 .symtab 与 .dynsym，因此可命中未导出的内部符号。
 * 该解析器在模块名为空时会遍历所有模块，代价很高，所以本定位器强制要求填写模块名。
 * 2026 BY white
 */
class SymbolLocator final : public TargetLocator {
public:
    [[nodiscard]] LocateResult Locate(const core::TargetSpec& spec) noexcept override;
};

}  // namespace easy::locator
