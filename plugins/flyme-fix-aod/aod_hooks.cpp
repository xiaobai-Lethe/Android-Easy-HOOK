/**
 * Flyme 移植到小米：AOD 息屏点亮修复接入点。
 *
 * 背景：Flyme 自带的光学指纹/AOD 框架靠写 /sys/kernel/display_drivers/ofp_press_status 等
 * 节点点亮，这些节点在小米硬件上不存在。本插件改为纯属性检测 AOD
 * （persist.sys.meizu.aod + debug.tracing.screen_state），把点亮状态写到小米真实节点
 * （默认 mi_display 的 doze_brightness，可用属性覆盖）。
 *
 * system_server 入口统一走 flyme-common 的分发器(只 hook 一次 JNI_OnLoad)，
 * 避免与 flyme-fix-vibrator 等同样需要 system_server 入口的插件在同一符号上撞钩。
 */

#include "easy_hook/core/log.hpp"
#include "flyme-common/system_server_init.hpp"
#include "flyme-fix-aod/doze_monitor.hpp"

// 总开关：置 0 则不注册，插件退化为空壳。
#define FLYME_AOD_FIX_ENABLED 1

#if FLYME_AOD_FIX_ENABLED && defined(__ANDROID__)

#include <jni.h>

namespace {

// 由 flyme-common 分发器在 system_server 的 JNI_OnLoad 时调用(每进程一次)。
// AOD 桥接是纯 native 线程，不需要 env。
void OnSystemServerInit(JNIEnv*) noexcept {
    const flyme_aod::DozeMonitorStatus status = flyme_aod::DozeMonitor::Start();
    easy::core::Log::Print(easy::core::LogLevel::kInfo, "flyme-aod",
                           "system_server init, doze monitor=%s",
                           flyme_aod::DozeMonitorStatusName(status));
}

const flyme_common::SystemServerInitRegistrar kRegistrar{&OnSystemServerInit};

}  // namespace

#endif  // FLYME_AOD_FIX_ENABLED && __ANDROID__
