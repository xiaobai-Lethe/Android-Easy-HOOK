#include "flyme-FOD-xiaomi/pointer/session_pointer_transaction.hpp"

#include <string>

namespace fod::pointer {
namespace {

constexpr int kPointerDownTransaction = 12;
constexpr int kPointerUpTransaction = 13;

SessionPointerTransactionResult Failed(
    SessionPointerTransactionStatus status, int values_written = 0,
    bool transacted = false) noexcept {
    return SessionPointerTransactionResult{
        .status = status,
        .values_written = values_written,
        .transacted = transacted,
    };
}

bool IsValidRequest(const SessionPointerFallbackRequest& request) noexcept {
    if (request.descriptor.empty()) {
        return false;
    }
    return request.write_axes
               ? request.transaction_code == kPointerDownTransaction
               : request.transaction_code == kPointerUpTransaction;
}

SessionPointerTransactionResult FailAfterRecycle(
    SessionPointerTransactionDriver& driver,
    SessionPointerTransactionStatus status, int values_written = 0,
    bool transacted = false) noexcept {
    driver.Recycle();
    return Failed(status, values_written, transacted);
}

#if defined(__ANDROID__)
using BinderProxyTransactFunction = jboolean (*)(
    JNIEnv*, jobject, jint, jobject, jobject, jint);

bool ConsumeJniException(JNIEnv* env) noexcept {
    if (env == nullptr || env->ExceptionCheck() != JNI_TRUE) {
        return false;
    }
    env->ExceptionClear();
    return true;
}

class JniSessionPointerTransactionDriver final
    : public SessionPointerTransactionDriver {
public:
    JniSessionPointerTransactionDriver(
        JNIEnv* env, jobject session_proxy,
        void* original_binder_transact) noexcept
        : env_(env),
          session_proxy_(session_proxy),
          original_binder_transact_(reinterpret_cast<BinderProxyTransactFunction>(
              original_binder_transact)) {}

    ~JniSessionPointerTransactionDriver() override {
        Recycle();
    }

    bool ObtainParcels(bool require_float) noexcept override {
        if (env_ == nullptr || session_proxy_ == nullptr ||
            original_binder_transact_ == nullptr) {
            return false;
        }

        parcel_class_ = env_->FindClass("android/os/Parcel");
        if (ConsumeJniException(env_) || parcel_class_ == nullptr) {
            return false;
        }

        obtain_ = env_->GetStaticMethodID(
            parcel_class_, "obtain", "()Landroid/os/Parcel;");
        ConsumeJniException(env_);
        recycle_ = env_->GetMethodID(parcel_class_, "recycle", "()V");
        ConsumeJniException(env_);
        write_interface_token_ = env_->GetMethodID(
            parcel_class_, "writeInterfaceToken", "(Ljava/lang/String;)V");
        ConsumeJniException(env_);
        write_int_ = env_->GetMethodID(parcel_class_, "writeInt", "(I)V");
        ConsumeJniException(env_);
        write_float_ = env_->GetMethodID(parcel_class_, "writeFloat", "(F)V");
        ConsumeJniException(env_);
        read_exception_ = env_->GetMethodID(
            parcel_class_, "readException", "()V");
        ConsumeJniException(env_);
        if (obtain_ == nullptr || recycle_ == nullptr ||
            write_interface_token_ == nullptr || write_int_ == nullptr ||
            read_exception_ == nullptr ||
            (require_float && write_float_ == nullptr)) {
            return false;
        }

        data_ = env_->CallStaticObjectMethod(parcel_class_, obtain_);
        if (ConsumeJniException(env_) || data_ == nullptr) {
            return false;
        }
        reply_ = env_->CallStaticObjectMethod(parcel_class_, obtain_);
        return !ConsumeJniException(env_) && reply_ != nullptr;
    }

    bool WriteInterfaceToken(std::string_view descriptor) noexcept override {
        if (env_ == nullptr || data_ == nullptr ||
            write_interface_token_ == nullptr) {
            return false;
        }

        std::string descriptor_string;
        try {
            descriptor_string.assign(descriptor);
        } catch (...) {
            return false;
        }

        jstring token = env_->NewStringUTF(descriptor_string.c_str());
        if (ConsumeJniException(env_) || token == nullptr) {
            return false;
        }
        env_->CallVoidMethod(data_, write_interface_token_, token);
        env_->DeleteLocalRef(token);
        return !ConsumeJniException(env_);
    }

    bool WriteInt(int value) noexcept override {
        if (env_ == nullptr || data_ == nullptr || write_int_ == nullptr) {
            return false;
        }
        env_->CallVoidMethod(data_, write_int_, static_cast<jint>(value));
        return !ConsumeJniException(env_);
    }

    bool WriteFloat(float value) noexcept override {
        if (env_ == nullptr || data_ == nullptr || write_float_ == nullptr) {
            return false;
        }
        env_->CallVoidMethod(data_, write_float_, static_cast<jfloat>(value));
        return !ConsumeJniException(env_);
    }

    bool Transact(int code) noexcept override {
        if (env_ == nullptr || session_proxy_ == nullptr || data_ == nullptr ||
            reply_ == nullptr || original_binder_transact_ == nullptr) {
            return false;
        }

        const jboolean result = original_binder_transact_(
            env_, session_proxy_, static_cast<jint>(code), data_, reply_, 0);
        return !ConsumeJniException(env_) && result == JNI_TRUE;
    }

    bool ReadException() noexcept override {
        if (env_ == nullptr || reply_ == nullptr || read_exception_ == nullptr) {
            return false;
        }
        env_->CallVoidMethod(reply_, read_exception_);
        return !ConsumeJniException(env_);
    }

    void Recycle() noexcept override {
        if (env_ == nullptr) {
            return;
        }

        RecycleParcel(data_);
        RecycleParcel(reply_);
        if (parcel_class_ != nullptr) {
            env_->DeleteLocalRef(parcel_class_);
        }

        data_ = nullptr;
        reply_ = nullptr;
        parcel_class_ = nullptr;
        obtain_ = nullptr;
        recycle_ = nullptr;
        write_interface_token_ = nullptr;
        write_int_ = nullptr;
        write_float_ = nullptr;
        read_exception_ = nullptr;
    }

private:
    void RecycleParcel(jobject parcel) noexcept {
        if (parcel == nullptr) {
            return;
        }
        if (recycle_ != nullptr) {
            env_->CallVoidMethod(parcel, recycle_);
            ConsumeJniException(env_);
        }
        env_->DeleteLocalRef(parcel);
    }

    JNIEnv* env_{};
    jobject session_proxy_{};
    BinderProxyTransactFunction original_binder_transact_{};
    jclass parcel_class_{};
    jobject data_{};
    jobject reply_{};
    jmethodID obtain_{};
    jmethodID recycle_{};
    jmethodID write_interface_token_{};
    jmethodID write_int_{};
    jmethodID write_float_{};
    jmethodID read_exception_{};
};
#endif

}  // namespace

SessionPointerTransactionResult SessionPointerTransactionRunner::Run(
    SessionPointerTransactionDriver& driver,
    const SessionPointerFallbackRequest& request) noexcept {
    if (!IsValidRequest(request)) {
        return Failed(SessionPointerTransactionStatus::kInvalidRequest);
    }
    if (!driver.ObtainParcels(request.write_axes)) {
        return FailAfterRecycle(
            driver, SessionPointerTransactionStatus::kParcelUnavailable);
    }
    if (!driver.WriteInterfaceToken(request.descriptor)) {
        return FailAfterRecycle(
            driver, SessionPointerTransactionStatus::kWriteFailed);
    }

    int values_written = 0;
    const auto write_int = [&driver, &values_written](int value) noexcept {
        if (!driver.WriteInt(value)) {
            return false;
        }
        ++values_written;
        return true;
    };
    const auto write_float = [&driver, &values_written](float value) noexcept {
        if (!driver.WriteFloat(value)) {
            return false;
        }
        ++values_written;
        return true;
    };

    if (!write_int(request.pointer_id)) {
        return FailAfterRecycle(
            driver, SessionPointerTransactionStatus::kWriteFailed,
            values_written);
    }

    if (request.write_axes) {
        if (!write_int(request.x) || !write_int(request.y) ||
            !write_float(request.minor) || !write_float(request.major)) {
            return FailAfterRecycle(
                driver, SessionPointerTransactionStatus::kWriteFailed,
                values_written);
        }
    }

    if (!driver.Transact(request.transaction_code)) {
        return FailAfterRecycle(
            driver, SessionPointerTransactionStatus::kTransactFailed,
            values_written);
    }
    if (!driver.ReadException()) {
        return FailAfterRecycle(
            driver, SessionPointerTransactionStatus::kRemoteException,
            values_written, true);
    }

    driver.Recycle();
    return SessionPointerTransactionResult{
        .status = SessionPointerTransactionStatus::kTransacted,
        .values_written = values_written,
        .transacted = true,
    };
}

SessionPointerTransactionResult SessionPointerTransaction::Transact(
    JNIEnv* env, jobject session_proxy, void* original_binder_transact,
    const SessionPointerFallbackRequest& request) noexcept {
    if (env == nullptr || session_proxy == nullptr ||
        original_binder_transact == nullptr) {
        return Failed(SessionPointerTransactionStatus::kInvalidRequest);
    }

#if defined(__ANDROID__)
    JniSessionPointerTransactionDriver driver(
        env, session_proxy, original_binder_transact);
    return SessionPointerTransactionRunner::Run(driver, request);
#else
    (void)request;
    return Failed(SessionPointerTransactionStatus::kInvalidRequest);
#endif
}

}  // namespace fod::pointer
