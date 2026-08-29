#pragma once

#include "flyme-FOD-xiaomi/udfps/geometry.hpp"
#include "flyme-FOD-xiaomi/jni/jni_env.hpp"
#include "flyme-FOD-xiaomi/udfps/patch_result.hpp"

namespace fod::udfps {

// 传感器关键字段的可测试镜像。
struct SensorPropertyRecord final {
    int sensor_id{};
    int sensor_type{};
    bool hal_controls_illumination{true};
    bool has_sensor_type_field{true};
    bool has_illumination_field{true};
    bool has_location_field{true};
};

// 只修改 id 匹配的传感器，避免影响侧边或后置指纹。
struct SensorPatchConfig final {
    int target_sensor_id{};
    Geometry geometry{};
};

// 将一个 Java 传感器对象改成目标光学屏下指纹配置。
class SensorPatcher final {
public:
    SensorPatcher() = delete;

    [[nodiscard]] static PatchResult PatchJavaSensor(JNIEnv* env, jobject sensor,
                                                     const SensorPatchConfig& config) noexcept;
};

}  // namespace fod::udfps
