#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"
#include "flyme-FOD-xiaomi/udfps/sensor_patcher.hpp"

namespace fod::udfps {

// 修改 system_server 中已经创建的传感器属性和 BiometricScheduler 类型。
class SchedulerPatcher final {
public:
    SchedulerPatcher() = delete;

    [[nodiscard]] static PatchResult PatchJavaService(JNIEnv* env, jobject service,
                                                      const SensorPatchConfig& config) noexcept;
};

}  // namespace fod::udfps
