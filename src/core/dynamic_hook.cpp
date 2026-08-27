#include "easy_hook/dynamic_hook.hpp"

#include "easy_hook/backend/inline_hook_backend.hpp"
#include "easy_hook/core/log.hpp"
#include "easy_hook/module/module_lookup.hpp"

#include <string_view>

#if defined(__ANDROID__)
#include "dobby.h"
#endif

namespace easy {
namespace {

constexpr std::string_view kLogTag = "dynamic-hook";

}  // namespace

bool HookFunction(void* target, void* replacement, void** original_slot) noexcept {
    if (target == nullptr || replacement == nullptr || original_slot == nullptr) {
        core::Log::Print(core::LogLevel::kError, kLogTag,
                         "hook rejected: target=%p replacement=%p slot=%p", target, replacement,
                         static_cast<void*>(original_slot));
        return false;
    }

    const bool installed = backend::DefaultInlineHookBackend().Hook(
        reinterpret_cast<std::uintptr_t>(target), replacement, original_slot);
    core::Log::Print(installed ? core::LogLevel::kInfo : core::LogLevel::kError, kLogTag,
                     "hook address=%p result=%d", target, installed ? 1 : 0);
    return installed;
}

bool HookSymbol(const char* module_name, const char* symbol_name, void* replacement,
                void** original_slot) noexcept {
    void* target = ResolveSymbol(module_name, symbol_name);
    if (target == nullptr) {
        core::Log::Print(core::LogLevel::kError, kLogTag, "symbol %s not found in %s",
                         symbol_name != nullptr ? symbol_name : "?",
                         module_name != nullptr ? module_name : "?");
        return false;
    }
    return HookFunction(target, replacement, original_slot);
}

bool UnhookFunction(void* target) noexcept {
    if (target == nullptr) {
        return false;
    }
    const bool removed =
        backend::DefaultInlineHookBackend().Unhook(reinterpret_cast<std::uintptr_t>(target));
    core::Log::Print(removed ? core::LogLevel::kInfo : core::LogLevel::kWarning, kLogTag,
                     "unhook address=%p result=%d", target, removed ? 1 : 0);
    return removed;
}

void* ResolveSymbol(const char* module_name, const char* symbol_name) noexcept {
    if (module_name == nullptr || module_name[0] == '\0' || symbol_name == nullptr ||
        symbol_name[0] == '\0') {
        return nullptr;
    }

    /**
     * 2026年08月27日22时42分
     * 先确认模块已加载再解析。底层解析器在模块未命中时会遍历并映射全部已加载模块，
     * 在启动早期反复触发这种全量扫描会显著拖慢进程。
     * 2026 BY white
     */
    if (!module::ModuleLookup::IsLoaded(std::string_view(module_name))) {
        return nullptr;
    }

#if defined(__ANDROID__)
    return DobbySymbolResolver(module_name, symbol_name);
#else
    return nullptr;
#endif
}

}  // namespace easy
