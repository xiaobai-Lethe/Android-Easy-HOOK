#pragma once

#include "fod/jni/jni_env.hpp"

#include <string_view>

namespace fod::jni {

// 从 ServiceManager 取得 IBinder，并转换为调用方需要的服务接口。
class ServiceResolver final {
public:
    static constexpr std::string_view kFingerprintServiceName = "fingerprint";
    static constexpr std::string_view kFingerprintServiceStubClass =
        "android/hardware/fingerprint/IFingerprintService$Stub";

    explicit ServiceResolver(JNIEnv* env, jobject preferred_loader = nullptr) noexcept;

    // 成功时返回局部引用，调用方负责释放。
    [[nodiscard]] jobject GetService(std::string_view service_name) const noexcept;
    [[nodiscard]] jobject GetFingerprintService() const noexcept;

    // 控制字符和空白会导致服务查找含义不明确，因此直接拒绝。
    [[nodiscard]] static bool IsValidServiceName(std::string_view service_name) noexcept;

private:
    JNIEnv* env_{};
    jobject preferred_loader_{};
};

}  // namespace fod::jni
