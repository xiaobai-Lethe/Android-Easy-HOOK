#include "easy_hook/locator/symbol_locator.hpp"

#include "easy_hook/module/module_lookup.hpp"

#include <string_view>

#if defined(__ANDROID__)
#include "dobby.h"
#endif

namespace easy::locator {

LocateResult SymbolLocator::Locate(const core::TargetSpec& spec) noexcept {
    if (spec.module == nullptr || spec.module[0] == '\0' || spec.symbol == nullptr ||
        spec.symbol[0] == '\0') {
        return LocateResult::Failure(core::InstallState::kInvalidSpec);
    }

    /**
     * 2026年08月27日21时48分
     * 先确认模块已加载。这样才能把"模块还没加载"与"模块已加载但符号不存在"区分开，
     * 前者可在模块加载后重试，后者是用户填错了符号名，重试没有意义。
     * 2026 BY white
     */
    if (!module::ModuleLookup::IsLoaded(std::string_view(spec.module))) {
        return LocateResult::Failure(core::InstallState::kModuleNotLoaded);
    }

#if defined(__ANDROID__)
    void* address = DobbySymbolResolver(spec.module, spec.symbol);
    if (address == nullptr) {
        return LocateResult::Failure(core::InstallState::kSymbolNotFound);
    }
    return LocateResult::Success(reinterpret_cast<std::uintptr_t>(address));
#else
    return LocateResult::Failure(core::InstallState::kSymbolNotFound);
#endif
}

}  // namespace easy::locator
