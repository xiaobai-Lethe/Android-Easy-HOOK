#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace fod::binder {

// 读取接口描述符时必须恢复 Parcel 的原始位置，否则系统后续无法正常解析事务。
enum class ParcelDescriptorStatus : std::uint8_t {
    kRead,
    kMissingParcel,
    kDataPositionFailed,
    kSetDataPositionFailed,
    kReadStringFailed,
    kRestorePositionFailed,
};

struct ParcelDescriptorResult final {
    ParcelDescriptorStatus status{ParcelDescriptorStatus::kMissingParcel};
    bool restored_position{};
    std::string descriptor;
};

class ParcelDescriptorBackend {
public:
    virtual ~ParcelDescriptorBackend() = default;

    [[nodiscard]] virtual bool ReadDataPosition(int& position) noexcept = 0;
    [[nodiscard]] virtual bool SetDataPosition(int position) noexcept = 0;
    [[nodiscard]] virtual bool ReadString(std::string& value) noexcept = 0;
};

// 执行“保存位置、回到开头读取、恢复位置”，并限制异常描述符的长度。
class ParcelDescriptorReader final {
public:
    static constexpr std::size_t kMaxDescriptorLength = 255;

    ParcelDescriptorReader() = delete;

    [[nodiscard]] static ParcelDescriptorResult ReadInterfaceDescriptor(
        ParcelDescriptorBackend& backend);
};

// JNI 便捷入口。内部错误会转换为状态码，不向 hook 调用栈抛出异常。
[[nodiscard]] ParcelDescriptorResult ReadParcelInterfaceDescriptor(
    JNIEnv* env, jobject parcel) noexcept;

}  // namespace fod::binder
