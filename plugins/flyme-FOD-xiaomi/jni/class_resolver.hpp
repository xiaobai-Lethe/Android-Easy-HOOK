#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace fod::jni {

// 在多个 ClassLoader 中查找类，用于处理厂商框架类不在启动类路径中的情况。
class ClassResolver final {
public:
    // 限制线程扫描数量，避免类查找阻塞 system_server 太久。
    static constexpr std::size_t kMaxLiveThreadLoaders = 32;

    explicit ClassResolver(JNIEnv* env, jobject preferred_loader = nullptr) noexcept;

    // 返回 jclass 局部引用；失败时返回 nullptr 并清除查找产生的 Java 异常。
    [[nodiscard]] jclass Resolve(std::string_view binary_name) const noexcept;

    [[nodiscard]] static std::string ToDotName(std::string_view binary_name);
    [[nodiscard]] static std::string ToSlashName(std::string_view binary_name);
    // 拒绝空段、混用分隔符和 JNI 不接受的字符。
    [[nodiscard]] static bool IsValidBinaryName(std::string_view binary_name) noexcept;

private:
    JNIEnv* env_{};
    jobject preferred_loader_{};
};

}  // namespace fod::jni
