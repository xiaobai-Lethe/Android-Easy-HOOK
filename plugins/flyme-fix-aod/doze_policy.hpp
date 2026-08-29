#pragma once

namespace flyme_aod {

/**
 * AOD 点亮判据的输入，全部来自系统属性，无需 JNI/ContentResolver。
 *   screen_state: debug.tracing.screen_state —— 2=亮 3=doze 4=doze_suspend 1=灭
 *   aod_enabled:  persist.sys.meizu.aod —— Flyme 息屏显示总开关
 */
struct DozePolicyInput final {
    int screen_state{0};
    bool aod_enabled{false};
};

struct DozePolicyDecision final {
    bool screen_is_doze{false};
    bool should_light{false};
};

// 纯函数式判据，便于单测；不触碰任何系统状态。
class DozePolicy final {
public:
    DozePolicy() = delete;

    [[nodiscard]] static bool IsDozeScreenState(int screen_state) noexcept;
    [[nodiscard]] static DozePolicyDecision Evaluate(const DozePolicyInput& input) noexcept;
};

}  // namespace flyme_aod
