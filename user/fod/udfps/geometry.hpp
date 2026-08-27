#pragma once

#include <string_view>

namespace fod::udfps {

// 从厂商属性得到的传感器位置；解析失败的部分会使用内置默认值。
struct Geometry final {
    int raw_x{};
    int raw_y{};
    int width{};
    int height{};
    int center_x{};
    int center_y{};
    int radius{};
    bool used_default_location{};
    bool used_default_size{};

    // location 和 size 分别解析，单项失败不会丢弃另一项的有效值。
    [[nodiscard]] static Geometry FromPropertyStrings(std::string_view location,
                                                      std::string_view size) noexcept;
};

// Android framework 中光学屏下指纹和对应调度器类型的固定枚举值。
inline constexpr int kFingerprintSensorTypeUdfpsOptical = 3;
inline constexpr int kBiometricSchedulerTypeUdfps = 2;

}  // namespace fod::udfps
