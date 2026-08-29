#include "flyme-FOD-xiaomi/pointer/pointer_context.hpp"

namespace fod::pointer {
namespace {

constexpr int kOnPointerDownCode = 12;
constexpr int kOnPointerUpCode = 13;

}  // namespace

SessionPointerFallbackRequest SessionPointerFallback::Build(
    PointerEventKind event_kind, const PointerContext& context) noexcept {
    const bool pointer_down = event_kind == PointerEventKind::kDown;
    return SessionPointerFallbackRequest{
        .transaction_code = pointer_down ? kOnPointerDownCode : kOnPointerUpCode,
        .pointer_id = context.pointer_id,
        .x = static_cast<int>(context.x),
        .y = static_cast<int>(context.y),
        .minor = context.minor,
        .major = context.major,
        .write_axes = pointer_down,
        .descriptor = SessionPointerFallbackRequest::kDescriptor,
    };
}

}  // namespace fod::pointer
