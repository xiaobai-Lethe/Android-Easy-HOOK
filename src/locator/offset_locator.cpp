#include "easy_hook/locator/offset_locator.hpp"

#include "easy_hook/core/log.hpp"
#include "easy_hook/module/module_lookup.hpp"

#include <optional>
#include <string_view>

namespace easy::locator {
namespace {

constexpr std::string_view kLogTag = "offset-locator";

}  // namespace

LocateResult OffsetLocator::Locate(const core::TargetSpec& spec) noexcept {
    if (spec.module == nullptr || spec.module[0] == '\0' || spec.offset == 0) {
        return LocateResult::Failure(core::InstallState::kInvalidSpec);
    }

    const std::optional<module::ModuleInfo> module_info =
        module::ModuleLookup::Find(std::string_view(spec.module));
    if (!module_info.has_value()) {
        return LocateResult::Failure(core::InstallState::kModuleNotLoaded);
    }

    /**
     * 2026年08月27日21时48分
     * 偏移按 IDA 中显示的相对虚拟地址理解，用模块 load_bias 换算成运行时绝对地址。
     * 这里必须用 load_bias 而不是 /proc/self/maps 的首行基址：模块存在多个 PT_LOAD 段时
     * 两者并不相等，直接用 maps 基址会算出错误地址。
     * 2026 BY white
     */
    if (!module_info->ContainsRelativeAddress(spec.offset)) {
        core::Log::Print(core::LogLevel::kError, kLogTag,
                         "offset 0x%zx is outside any mapped segment of %s",
                         static_cast<std::size_t>(spec.offset), module_info->path.c_str());
        return LocateResult::Failure(core::InstallState::kOffsetOutOfRange);
    }

    const std::uintptr_t address = module_info->ToRuntimeAddress(spec.offset);
    core::Log::Print(core::LogLevel::kDebug, kLogTag,
                     "resolved %s offset=0x%zx load_bias=%p address=%p",
                     module_info->path.c_str(), static_cast<std::size_t>(spec.offset),
                     reinterpret_cast<void*>(module_info->load_bias),
                     reinterpret_cast<void*>(address));
    return LocateResult::Success(address);
}

}  // namespace easy::locator
