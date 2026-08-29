#pragma once

namespace flyme_aod {

// 区分“真的启动了”和“按开关/状态跳过”，便于日志如实反映。
enum class DozeMonitorStatus {
    kStarted,
    kAlreadyStarted,
    kDisabled,
    kThreadStartFailed,
    kUnsupportedPlatform,
};

[[nodiscard]] const char* DozeMonitorStatusName(DozeMonitorStatus status) noexcept;

/**
 * AOD 息屏点亮桥接：Flyme 移植到小米后，原本写 /sys/kernel/display_drivers/ofp_press_status
 * 等节点在小米机上不存在，这里改为纯属性检测 AOD，并把点亮状态写到小米真实节点
 * （默认 mi_display 的 doze_brightness，可用属性覆盖）。
 *
 * 只应在 system_server 调用（写显示节点的权限与 AOD 策略都在这里）。幂等，重复调用只首次生效。
 */
class DozeMonitor final {
public:
    DozeMonitor() = delete;

    static DozeMonitorStatus Start() noexcept;
};

}  // namespace flyme_aod
