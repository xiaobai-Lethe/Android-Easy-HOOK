#pragma once

#include <string>
#include <string_view>

namespace fod::platform::android {

// Android 系统属性的读取和常用类型转换；解析失败时统一返回默认值。
class SystemProperties final {
public:
    static constexpr std::string_view kUdfpsArtHookProperty =
        "persist.fod.udfps_art_hook";
    static constexpr std::string_view kUdfpsSensorIdProperty =
        "persist.fod.udfps_sensor_id";
    static constexpr std::string_view kAodDozeMonitorProperty =
        "persist.fod.aod_doze_monitor";
    static constexpr std::string_view kFodLocationProperty =
        "persist.vendor.sys.fp.fod.location.X_Y";
    static constexpr std::string_view kFodSizeProperty =
        "persist.vendor.sys.fp.fod.size.width_height";

    SystemProperties() = delete;

    // 支持 1/0、true/false、yes/no 和 on/off，大小写不敏感。
    [[nodiscard]] static bool ParseBool(std::string_view value,
                                        bool default_value) noexcept;
    // 只有去除首尾空白后整段都是合法整数才算成功。
    [[nodiscard]] static int ParseInt(std::string_view value, int default_value) noexcept;
    [[nodiscard]] static std::string GetString(std::string_view name,
                                               std::string_view default_value);
    [[nodiscard]] static bool GetBool(std::string_view name, bool default_value) noexcept;
    [[nodiscard]] static int GetInt(std::string_view name, int default_value) noexcept;
};

}  // namespace fod::platform::android
