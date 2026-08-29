#include "flyme-fix-aod/doze_policy.hpp"

namespace flyme_aod {
namespace {

constexpr int kScreenStateDoze = 3;
constexpr int kScreenStateDozeSuspend = 4;

}  // namespace

bool DozePolicy::IsDozeScreenState(int screen_state) noexcept {
    return screen_state == kScreenStateDoze || screen_state == kScreenStateDozeSuspend;
}

DozePolicyDecision DozePolicy::Evaluate(const DozePolicyInput& input) noexcept {
    DozePolicyDecision decision;
    decision.screen_is_doze = IsDozeScreenState(input.screen_state);
    // AOD 开着且屏幕处于 doze 才点亮；其余一律熄灭。
    decision.should_light = input.aod_enabled && decision.screen_is_doze;
    return decision;
}

}  // namespace flyme_aod
