#include "easy_hook/core/install_state.hpp"

namespace easy::core {

const char* InstallStateName(InstallState state) noexcept {
    switch (state) {
        case InstallState::kNotAttempted:
            return "not_attempted";
        case InstallState::kInstalled:
            return "installed";
        case InstallState::kSkippedProcessFilter:
            return "skipped_process_filter";
        case InstallState::kInvalidSpec:
            return "invalid_spec";
        case InstallState::kModuleNotLoaded:
            return "module_not_loaded";
        case InstallState::kSymbolNotFound:
            return "symbol_not_found";
        case InstallState::kOffsetOutOfRange:
            return "offset_out_of_range";
        case InstallState::kPatternInvalid:
            return "pattern_invalid";
        case InstallState::kPatternNotFound:
            return "pattern_not_found";
        case InstallState::kPatternAmbiguous:
            return "pattern_ambiguous";
        case InstallState::kBackendFailed:
            return "backend_failed";
    }
    return "invalid";
}

bool IsRetryableState(InstallState state) noexcept {
    return state == InstallState::kNotAttempted || state == InstallState::kModuleNotLoaded;
}

}  // namespace easy::core
