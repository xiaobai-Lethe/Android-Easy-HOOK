#include "easy_hook/pattern/byte_pattern.hpp"

#include <cctype>

namespace easy::pattern {
namespace {

constexpr int kInvalidNibble = -1;

/**
 * 2026年08月27日21时48分
 * 把一个十六进制字符转换为 0 到 15 的数值，非法字符返回 kInvalidNibble。
 * 2026 BY white
 */
int HexNibble(char character) noexcept {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return kInvalidNibble;
}

bool IsSeparator(char character) noexcept {
    return character == ' ' || character == '\t' || character == ',' || character == '\r' ||
           character == '\n';
}

}  // namespace

bool BytePattern::Parse(std::string_view text) noexcept {
    bytes_.clear();
    wildcard_.clear();

    std::size_t index = 0;
    while (index < text.size()) {
        if (IsSeparator(text[index])) {
            ++index;
            continue;
        }

        if (bytes_.size() >= kMaxPatternLength) {
            bytes_.clear();
            wildcard_.clear();
            return false;
        }

        if (text[index] == '?') {
            ++index;
            if (index < text.size() && text[index] == '?') {
                ++index;
            }
            bytes_.push_back(0);
            wildcard_.push_back(true);
            continue;
        }

        const int high = HexNibble(text[index]);
        if (high == kInvalidNibble) {
            bytes_.clear();
            wildcard_.clear();
            return false;
        }
        ++index;

        int value = high;
        if (index < text.size() && !IsSeparator(text[index])) {
            const int low = HexNibble(text[index]);
            if (low == kInvalidNibble) {
                bytes_.clear();
                wildcard_.clear();
                return false;
            }
            value = (high << 4) | low;
            ++index;
        }

        bytes_.push_back(static_cast<std::uint8_t>(value));
        wildcard_.push_back(false);
    }

    if (bytes_.empty()) {
        return false;
    }

    /**
     * 2026年08月27日21时48分
     * 首字节为通配会让搜索退化为逐字节全量比对，且极易产生大量误命中，直接拒绝。
     * 2026 BY white
     */
    if (wildcard_.front()) {
        bytes_.clear();
        wildcard_.clear();
        return false;
    }
    return true;
}

bool BytePattern::MatchesAt(const std::uint8_t* memory) const noexcept {
    if (memory == nullptr || bytes_.empty()) {
        return false;
    }
    for (std::size_t index = 0; index < bytes_.size(); ++index) {
        if (wildcard_[index]) {
            continue;
        }
        if (memory[index] != bytes_[index]) {
            return false;
        }
    }
    return true;
}

PatternScanResult PatternScanner::ScanExecutableSegments(const module::ModuleInfo& module_info,
                                                        const BytePattern& pattern) noexcept {
    PatternScanResult result;
    if (pattern.empty()) {
        return result;
    }

    const std::size_t pattern_size = pattern.size();
    for (const module::ModuleSegment& segment : module_info.segments) {
        if (!segment.executable || segment.file_size < pattern_size) {
            continue;
        }

        const auto* begin = reinterpret_cast<const std::uint8_t*>(segment.start);
        const std::size_t last_offset = segment.file_size - pattern_size;
        for (std::size_t offset = 0; offset <= last_offset; ++offset) {
            if (!pattern.MatchesAt(begin + offset)) {
                continue;
            }
            if (result.match_count == 0) {
                result.address = segment.start + offset;
            }
            ++result.match_count;
            if (result.match_count >= kMaxReportedMatches) {
                return result;
            }
        }
    }
    return result;
}

}  // namespace easy::pattern
