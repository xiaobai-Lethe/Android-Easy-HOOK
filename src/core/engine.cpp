#include "easy_hook/core/engine.hpp"

#include "easy_hook/core/log.hpp"

#include <string_view>

namespace easy::core {
namespace {

constexpr std::string_view kLogTag = "engine";

}  // namespace

HookEngine& HookEngine::Instance() noexcept {
    static HookEngine instance(backend::DefaultInlineHookBackend());
    return instance;
}

HookEngine::HookEngine(backend::InlineHookBackend& hook_backend) noexcept
    : backend_(hook_backend) {}

locator::TargetLocator* HookEngine::LocatorFor(TargetKind kind) noexcept {
    switch (kind) {
        case TargetKind::kSymbol:
            return &symbol_locator_;
        case TargetKind::kOffset:
            return &offset_locator_;
        case TargetKind::kPattern:
            return &pattern_locator_;
    }
    return nullptr;
}

bool HookEngine::Install(Registration& registration) noexcept {
    const TargetSpec& spec = registration.spec();

    locator::TargetLocator* target_locator = LocatorFor(spec.kind);
    if (target_locator == nullptr) {
        registration.SetResult(InstallState::kInvalidSpec, 0);
        Log::Print(LogLevel::kError, kLogTag, "hook name=%s invalid target kind",
                   spec.name != nullptr ? spec.name : "?");
        return false;
    }

    const locator::LocateResult located = target_locator->Locate(spec);
    if (!located.resolved()) {
        registration.SetResult(located.state, 0);
        const LogLevel level =
            IsRetryableState(located.state) ? LogLevel::kDebug : LogLevel::kError;
        Log::Print(level, kLogTag, "hook name=%s locate failed kind=%s module=%s state=%s",
                   spec.name != nullptr ? spec.name : "?", TargetKindName(spec.kind),
                   spec.module != nullptr ? spec.module : "?", InstallStateName(located.state));
        return false;
    }

    if (!backend_.Hook(located.address, registration.replacement(),
                       registration.original_slot())) {
        registration.SetResult(InstallState::kBackendFailed, located.address);
        Log::Print(LogLevel::kError, kLogTag,
                   "hook name=%s backend=%s install failed address=%p",
                   spec.name != nullptr ? spec.name : "?", backend_.name(),
                   reinterpret_cast<void*>(located.address));
        return false;
    }

    registration.SetResult(InstallState::kInstalled, located.address);
    Log::Print(LogLevel::kInfo, kLogTag,
               "hook name=%s installed kind=%s module=%s address=%p backend=%s",
               spec.name != nullptr ? spec.name : "?", TargetKindName(spec.kind),
               spec.module != nullptr ? spec.module : "?",
               reinterpret_cast<void*>(located.address), backend_.name());
    return true;
}

}  // namespace easy::core
