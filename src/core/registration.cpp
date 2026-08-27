#include "easy_hook/core/registration.hpp"

#include "easy_hook/core/registry.hpp"

namespace easy::core {

Registration::Registration(const TargetSpec& spec, void* replacement,
                           void** original_slot) noexcept
    : spec_(spec), replacement_(replacement), original_slot_(original_slot) {
    Registry::Instance().Add(this);
}

void Registration::SetResult(InstallState state, std::uintptr_t resolved_address) noexcept {
    state_ = state;
    resolved_address_ = resolved_address;
}

}  // namespace easy::core
