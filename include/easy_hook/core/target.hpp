#pragma once

#include <cstdint>

namespace easy::core {

/**
 * 2026年08月27日21时46分
 * hook 目标的定位方式。三种方式共用同一份规格结构，由引擎按此字段选择定位器。
 * 2026 BY white
 */
enum class TargetKind : unsigned char {
    kSymbol,
    kOffset,
    kPattern,
};

/**
 * 2026年08月27日21时46分
 * 描述一个 hook 目标的完整定位信息。所有字符串成员均来自用户宏中的字面量，
 * 具有静态存储期，因此这里只保存指针而不复制，结构可安全长期持有。
 * 2026 BY white
 */
struct TargetSpec final {
    TargetKind kind{TargetKind::kSymbol};
    const char* name{nullptr};
    const char* module{nullptr};
    const char* symbol{nullptr};
    const char* pattern{nullptr};
    std::uintptr_t offset{0};

    /**
     * 2026年08月27日21时46分
     * 构造按模块名加符号名定位的目标规格，符号可以是未导出的内部符号。
     * 2026 BY white
     */
    [[nodiscard]] static constexpr TargetSpec Symbol(const char* name, const char* module,
                                                     const char* symbol) noexcept {
        return TargetSpec{
            .kind = TargetKind::kSymbol,
            .name = name,
            .module = module,
            .symbol = symbol,
            .pattern = nullptr,
            .offset = 0,
        };
    }

    /**
     * 2026年08月27日21时46分
     * 构造按模块名加相对虚拟地址定位的目标规格，偏移取 IDA 中显示的地址。
     * 2026 BY white
     */
    [[nodiscard]] static constexpr TargetSpec Offset(const char* name, const char* module,
                                                     std::uintptr_t offset) noexcept {
        return TargetSpec{
            .kind = TargetKind::kOffset,
            .name = name,
            .module = module,
            .symbol = nullptr,
            .pattern = nullptr,
            .offset = offset,
        };
    }

    /**
     * 2026年08月27日21时46分
     * 构造按字节特征码定位的目标规格，特征码形如 "1F 20 ?? D5"，?? 表示任意字节。
     * 2026 BY white
     */
    [[nodiscard]] static constexpr TargetSpec Pattern(const char* name, const char* module,
                                                      const char* pattern) noexcept {
        return TargetSpec{
            .kind = TargetKind::kPattern,
            .name = name,
            .module = module,
            .symbol = nullptr,
            .pattern = pattern,
            .offset = 0,
        };
    }
};

/**
 * 2026年08月27日21时46分
 * 返回定位方式的可读名称，用于安装结果日志。
 * 2026 BY white
 */
[[nodiscard]] const char* TargetKindName(TargetKind kind) noexcept;

}  // namespace easy::core
