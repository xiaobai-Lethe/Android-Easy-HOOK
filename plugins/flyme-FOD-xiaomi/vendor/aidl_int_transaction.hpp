#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace fod::vendor {

// 描述“写入少量整数并读取一个整数”的 AIDL 调用，供指纹和触控服务共用。
struct AidlIntTransaction final {
    std::string_view service_name;
    std::string_view descriptor;
    int code{};
    std::array<int, 4> arguments{};
    std::size_t argument_count{};
};

// 按执行顺序细分失败位置，便于判断是服务、Parcel 还是远端调用出错。
enum class AidlIntTransactionStatus : std::uint8_t {
    kOk,
    kInvalidInput,
    kServiceUnavailable,
    kParcelUnavailable,
    kWriteFailed,
    kTransactFailed,
    kRemoteException,
    kReadFailed,
};

struct AidlIntTransactionResult final {
    bool ok{};
    int reply_value{};
    AidlIntTransactionStatus status{AidlIntTransactionStatus::kInvalidInput};
};

class AidlIntTransactionBackend {
public:
    virtual ~AidlIntTransactionBackend() = default;

    [[nodiscard]] virtual AidlIntTransactionResult Transact(
        const AidlIntTransaction& transaction) noexcept = 0;
};

// 将 Binder 调用拆成可测试步骤；Runner 负责在所有退出路径回收 Parcel。
class AidlIntTransactionDriver {
public:
    virtual ~AidlIntTransactionDriver() = default;

    [[nodiscard]] virtual bool PrepareService(std::string_view service_name) noexcept = 0;
    [[nodiscard]] virtual bool ObtainParcels() noexcept = 0;
    [[nodiscard]] virtual bool WriteInterfaceToken(std::string_view descriptor) noexcept = 0;
    [[nodiscard]] virtual bool WriteInt(int value) noexcept = 0;
    [[nodiscard]] virtual bool Transact(int code) noexcept = 0;
    [[nodiscard]] virtual bool ReadException() noexcept = 0;
    [[nodiscard]] virtual bool ReadInt(int& value) noexcept = 0;
    virtual void Recycle() noexcept = 0;
};

// 严格按准备服务、写参数、发起调用、读取结果的顺序执行。
class AidlIntTransactionRunner final {
public:
    AidlIntTransactionRunner() = delete;

    [[nodiscard]] static AidlIntTransactionResult Run(
        AidlIntTransactionDriver& driver, const AidlIntTransaction& transaction) noexcept;
};

// Android 使用 JNI 完成实际调用，宿主机构建返回服务不可用。
class JniAidlIntTransactionBackend final : public AidlIntTransactionBackend {
public:
    explicit JniAidlIntTransactionBackend(JNIEnv* env) noexcept;

    [[nodiscard]] AidlIntTransactionResult Transact(
        const AidlIntTransaction& transaction) noexcept override;

private:
    JNIEnv* env_{};
};

}  // namespace fod::vendor
