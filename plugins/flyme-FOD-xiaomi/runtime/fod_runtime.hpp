#pragma once

#include "flyme-FOD-xiaomi/core/process_context.hpp"
#include "flyme-FOD-xiaomi/jni/jni_env.hpp"
#include "flyme-FOD-xiaomi/udfps/geometry.hpp"

#include <cstdint>

namespace fod::runtime {

/**
 * 2026年08月27日23时54分
 * 屏下指纹修复的启动结果。区分"按配置或角色跳过"与"真的没能启动"。
 * 2026 BY white
 */
enum class FodStartStatus : std::uint8_t {
    kStarted,
    kAlreadyStarted,
    kSkippedFeatureDisabled,
    kSkippedProcessRole,
    kMissingJniEnv,
};

[[nodiscard]] const char* FodStartStatusName(FodStartStatus status) noexcept;

/**
 * 2026年08月27日23时54分
 * 从系统属性读出的运行配置。所有取值都在进程初始化时确定一次，之后不再变化。
 * 2026 BY white
 */
struct FodConfig final {
    core::ProcessRole role{core::ProcessRole::kUnknown};
    bool enabled{true};
    int target_sensor_id{0};
    udfps::Geometry geometry{};

    [[nodiscard]] static FodConfig ReadCurrent() noexcept;
};

/**
 * 2026年08月27日23时54分
 * 屏下指纹修复的总编排。
 *
 * 按进程角色分流：
 *   system_server 启动三个监控线程，负责修正传感器与调度器、管理 FOD 会话、处理卡死任务；
 *   Settings 与 SystemUI 只依赖 Binder 拦截识别按压事务，无需额外线程；
 *   其余进程完全不介入。
 * 2026 BY white
 */
class FodRuntime final {
public:
    FodRuntime() = delete;

    /**
     * 2026年08月27日23时54分
     * 在当前进程启动修复。可被重复调用，只有首次真正生效。
     * 服务尚未发布导致的失败不会置为终态，允许后续再次尝试。
     * 2026 BY white
     */
    static FodStartStatus Start(JNIEnv* env) noexcept;
};

}  // namespace fod::runtime
