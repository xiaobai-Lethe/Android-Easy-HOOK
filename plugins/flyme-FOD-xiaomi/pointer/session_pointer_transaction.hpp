#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"
#include "flyme-FOD-xiaomi/pointer/pointer_context.hpp"

#include <cstdint>
#include <string_view>

namespace fod::pointer {

// 会话关闭时直接向 ISession 补发 onPointerUp，避免 HAL 一直保留按下状态。
enum class SessionPointerTransactionStatus : std::uint8_t {
    kTransacted,
    kInvalidRequest,
    kParcelUnavailable,
    kWriteFailed,
    kTransactFailed,
    kRemoteException,
};

struct SessionPointerTransactionResult final {
    SessionPointerTransactionStatus status{
        SessionPointerTransactionStatus::kInvalidRequest};
    int values_written{};
    bool transacted{};
};

// 将 Parcel 操作拆开，保证失败路径也能被测试。
class SessionPointerTransactionDriver {
public:
    virtual ~SessionPointerTransactionDriver() = default;

    [[nodiscard]] virtual bool ObtainParcels(bool require_float) noexcept = 0;
    [[nodiscard]] virtual bool WriteInterfaceToken(
        std::string_view descriptor) noexcept = 0;
    [[nodiscard]] virtual bool WriteInt(int value) noexcept = 0;
    [[nodiscard]] virtual bool WriteFloat(float value) noexcept = 0;
    [[nodiscard]] virtual bool Transact(int code) noexcept = 0;
    [[nodiscard]] virtual bool ReadException() noexcept = 0;
    virtual void Recycle() noexcept = 0;
};

class SessionPointerTransactionRunner final {
public:
    SessionPointerTransactionRunner() = delete;

    [[nodiscard]] static SessionPointerTransactionResult Run(
        SessionPointerTransactionDriver& driver,
        const SessionPointerFallbackRequest& request) noexcept;
};

class SessionPointerTransaction final {
public:
    SessionPointerTransaction() = delete;

    [[nodiscard]] static SessionPointerTransactionResult Transact(
        JNIEnv* env, jobject session_proxy, void* original_binder_transact,
        const SessionPointerFallbackRequest& request) noexcept;
};

}  // namespace fod::pointer
