#pragma once

#include "easy_hook/module/module_lookup.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace easy::pattern {

/**
 * 2026年08月27日21时46分
 * 一条已解析的字节特征码。bytes 与 wildcard 等长，wildcard 为真的位置在匹配时被跳过。
 * 2026 BY white
 */
class BytePattern final {
public:
    static constexpr std::size_t kMaxPatternLength = 256;

    /**
     * 2026年08月27日21时46分
     * 解析形如 "1F 20 ?? D5" 的特征码文本。允许使用 ?? 或单个 ? 表示任意字节，
     * 分隔符可以是空格或制表符。解析失败时返回 false 并保持对象为空。
     * 2026 BY white
     */
    [[nodiscard]] bool Parse(std::string_view text) noexcept;

    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return bytes_.size(); }

    /**
     * 2026年08月27日21时46分
     * 判断从 memory 起始的字节序列是否匹配本特征码，调用方需保证可读长度不小于 size()。
     * 2026 BY white
     */
    [[nodiscard]] bool MatchesAt(const std::uint8_t* memory) const noexcept;

private:
    std::vector<std::uint8_t> bytes_;
    std::vector<bool> wildcard_;
};

/**
 * 2026年08月27日21时46分
 * 一次特征码搜索的结果。命中多于一处时视为特征码不够唯一，由引擎拒绝安装，
 * 以免在不同版本的目标库上 hook 到错误的位置。
 * 2026 BY white
 */
struct PatternScanResult final {
    std::uintptr_t address{0};
    std::size_t match_count{0};
};

/**
 * 2026年08月27日21时46分
 * 在模块的可执行段中搜索字节特征码。只扫描可执行段，既能排除数据段的偶然命中，
 * 也能显著缩小搜索范围。
 * 2026 BY white
 */
class PatternScanner final {
public:
    static constexpr std::size_t kMaxReportedMatches = 4;

    PatternScanner() = delete;

    [[nodiscard]] static PatternScanResult ScanExecutableSegments(
        const module::ModuleInfo& module_info, const BytePattern& pattern) noexcept;
};

}  // namespace easy::pattern
