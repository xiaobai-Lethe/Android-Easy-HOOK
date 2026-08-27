#include "easy_hook/locator/pattern_locator.hpp"

#include "easy_hook/core/log.hpp"
#include "easy_hook/module/module_lookup.hpp"
#include "easy_hook/pattern/byte_pattern.hpp"

#include <optional>
#include <string_view>

namespace easy::locator {
namespace {

constexpr std::string_view kLogTag = "pattern-locator";

}  // namespace

LocateResult PatternLocator::Locate(const core::TargetSpec& spec) noexcept {
    if (spec.module == nullptr || spec.module[0] == '\0' || spec.pattern == nullptr ||
        spec.pattern[0] == '\0') {
        return LocateResult::Failure(core::InstallState::kInvalidSpec);
    }

    pattern::BytePattern byte_pattern;
    if (!byte_pattern.Parse(std::string_view(spec.pattern))) {
        core::Log::Print(core::LogLevel::kError, kLogTag, "cannot parse pattern \"%s\"",
                         spec.pattern);
        return LocateResult::Failure(core::InstallState::kPatternInvalid);
    }

    const std::optional<module::ModuleInfo> module_info =
        module::ModuleLookup::Find(std::string_view(spec.module));
    if (!module_info.has_value()) {
        return LocateResult::Failure(core::InstallState::kModuleNotLoaded);
    }

    const pattern::PatternScanResult scan =
        pattern::PatternScanner::ScanExecutableSegments(*module_info, byte_pattern);
    if (scan.match_count == 0) {
        return LocateResult::Failure(core::InstallState::kPatternNotFound);
    }

    /**
     * 2026年08月27日21时48分
     * 命中多处说明特征码不够唯一。此时继续安装会在不同版本的目标库上 hook 到错误位置，
     * 因此直接拒绝，并提示用户加长特征码。
     * 2026 BY white
     */
    if (scan.match_count > 1) {
        core::Log::Print(core::LogLevel::kError, kLogTag,
                         "pattern matched %zu times in %s, please make it more specific",
                         scan.match_count, module_info->path.c_str());
        return LocateResult::Failure(core::InstallState::kPatternAmbiguous);
    }

    core::Log::Print(core::LogLevel::kDebug, kLogTag, "resolved %s pattern address=%p",
                     module_info->path.c_str(), reinterpret_cast<void*>(scan.address));
    return LocateResult::Success(scan.address);
}

}  // namespace easy::locator
