#include "easy_hook/core/target.hpp"

namespace easy::core {

const char* TargetKindName(TargetKind kind) noexcept {
    switch (kind) {
        case TargetKind::kSymbol:
            return "symbol";
        case TargetKind::kOffset:
            return "offset";
        case TargetKind::kPattern:
            return "pattern";
    }
    return "invalid";
}

}  // namespace easy::core
