#include "fod/vendor/aidl_int_transaction.hpp"

#include "fod/jni/service_resolver.hpp"

#include <string>

namespace fod::vendor {
namespace {

AidlIntTransactionResult Failed(AidlIntTransactionStatus status) noexcept {
    return AidlIntTransactionResult{
        .ok = false,
        .reply_value = 0,
        .status = status,
    };
}

bool IsValidTransaction(const AidlIntTransaction& transaction) noexcept {
    return !transaction.service_name.empty() && !transaction.descriptor.empty() &&
           transaction.code > 0 &&
           transaction.argument_count <= transaction.arguments.size();
}

AidlIntTransactionResult FailAfterRecycle(
    AidlIntTransactionDriver& driver, AidlIntTransactionStatus status) noexcept {
    driver.Recycle();
    return Failed(status);
}

#if defined(__ANDROID__)
void ClearJniException(JNIEnv* env) noexcept {
    if (env != nullptr && env->ExceptionCheck() == JNI_TRUE) {
        env->ExceptionClear();
    }
}

bool HasJniException(JNIEnv* env) noexcept {
    if (env == nullptr || env->ExceptionCheck() != JNI_TRUE) {
        return false;
    }
    env->ExceptionClear();
    return true;
}

class JniAidlIntTransactionDriver final : public AidlIntTransactionDriver {
public:
    explicit JniAidlIntTransactionDriver(JNIEnv* env) noexcept : env_(env) {}

    bool PrepareService(std::string_view service_name) noexcept override {
        if (env_ == nullptr) {
            return false;
        }

        jni::ServiceResolver resolver(env_);
        binder_ = resolver.GetService(service_name);
        return binder_ != nullptr;
    }

    bool ObtainParcels() noexcept override {
        if (env_ == nullptr || binder_ == nullptr) {
            return false;
        }

        parcel_class_ = env_->FindClass("android/os/Parcel");
        ClearJniException(env_);
        ibinder_class_ = env_->FindClass("android/os/IBinder");
        ClearJniException(env_);
        if (parcel_class_ == nullptr || ibinder_class_ == nullptr) {
            Recycle();
            return false;
        }

        obtain_ = env_->GetStaticMethodID(parcel_class_, "obtain", "()Landroid/os/Parcel;");
        ClearJniException(env_);
        recycle_ = env_->GetMethodID(parcel_class_, "recycle", "()V");
        ClearJniException(env_);
        write_interface_token_ =
            env_->GetMethodID(parcel_class_, "writeInterfaceToken", "(Ljava/lang/String;)V");
        ClearJniException(env_);
        write_int_ = env_->GetMethodID(parcel_class_, "writeInt", "(I)V");
        ClearJniException(env_);
        read_exception_ = env_->GetMethodID(parcel_class_, "readException", "()V");
        ClearJniException(env_);
        read_int_ = env_->GetMethodID(parcel_class_, "readInt", "()I");
        ClearJniException(env_);
        transact_ = env_->GetMethodID(ibinder_class_, "transact",
                                      "(ILandroid/os/Parcel;Landroid/os/Parcel;I)Z");
        ClearJniException(env_);
        if (obtain_ == nullptr || recycle_ == nullptr || write_interface_token_ == nullptr ||
            write_int_ == nullptr || read_exception_ == nullptr || read_int_ == nullptr ||
            transact_ == nullptr) {
            Recycle();
            return false;
        }

        data_ = env_->CallStaticObjectMethod(parcel_class_, obtain_);
        if (HasJniException(env_)) {
            Recycle();
            return false;
        }
        reply_ = env_->CallStaticObjectMethod(parcel_class_, obtain_);
        if (HasJniException(env_) || data_ == nullptr || reply_ == nullptr) {
            Recycle();
            return false;
        }
        return true;
    }

    bool WriteInterfaceToken(std::string_view descriptor) noexcept override {
        if (env_ == nullptr || data_ == nullptr || write_interface_token_ == nullptr) {
            return false;
        }

        std::string descriptor_string;
        try {
            descriptor_string.assign(descriptor);
        } catch (...) {
            return false;
        }

        jstring token = env_->NewStringUTF(descriptor_string.c_str());
        if (HasJniException(env_) || token == nullptr) {
            return false;
        }

        env_->CallVoidMethod(data_, write_interface_token_, token);
        env_->DeleteLocalRef(token);
        return !HasJniException(env_);
    }

    bool WriteInt(int value) noexcept override {
        if (env_ == nullptr || data_ == nullptr || write_int_ == nullptr) {
            return false;
        }

        env_->CallVoidMethod(data_, write_int_, static_cast<jint>(value));
        return !HasJniException(env_);
    }

    bool Transact(int code) noexcept override {
        if (env_ == nullptr || binder_ == nullptr || data_ == nullptr || reply_ == nullptr ||
            transact_ == nullptr) {
            return false;
        }

        const jboolean transacted =
            env_->CallBooleanMethod(binder_, transact_, static_cast<jint>(code), data_, reply_,
                                    static_cast<jint>(0));
        return !HasJniException(env_) && transacted == JNI_TRUE;
    }

    bool ReadException() noexcept override {
        if (env_ == nullptr || reply_ == nullptr || read_exception_ == nullptr) {
            return false;
        }

        env_->CallVoidMethod(reply_, read_exception_);
        return !HasJniException(env_);
    }

    bool ReadInt(int& value) noexcept override {
        if (env_ == nullptr || reply_ == nullptr || read_int_ == nullptr) {
            return false;
        }

        const jint reply_value = env_->CallIntMethod(reply_, read_int_);
        if (HasJniException(env_)) {
            return false;
        }
        value = static_cast<int>(reply_value);
        return true;
    }

    void Recycle() noexcept override {
        if (env_ == nullptr) {
            return;
        }

        if (data_ != nullptr && recycle_ != nullptr) {
            env_->CallVoidMethod(data_, recycle_);
            ClearJniException(env_);
        }
        if (reply_ != nullptr && recycle_ != nullptr) {
            env_->CallVoidMethod(reply_, recycle_);
            ClearJniException(env_);
        }
        if (binder_ != nullptr) {
            env_->DeleteLocalRef(binder_);
        }
        if (parcel_class_ != nullptr) {
            env_->DeleteLocalRef(parcel_class_);
        }
        if (ibinder_class_ != nullptr) {
            env_->DeleteLocalRef(ibinder_class_);
        }

        binder_ = nullptr;
        parcel_class_ = nullptr;
        ibinder_class_ = nullptr;
        data_ = nullptr;
        reply_ = nullptr;
        obtain_ = nullptr;
        recycle_ = nullptr;
        write_interface_token_ = nullptr;
        write_int_ = nullptr;
        read_exception_ = nullptr;
        read_int_ = nullptr;
        transact_ = nullptr;
    }

private:
    JNIEnv* env_{};
    jobject binder_{};
    jclass parcel_class_{};
    jclass ibinder_class_{};
    jobject data_{};
    jobject reply_{};
    jmethodID obtain_{};
    jmethodID recycle_{};
    jmethodID write_interface_token_{};
    jmethodID write_int_{};
    jmethodID read_exception_{};
    jmethodID read_int_{};
    jmethodID transact_{};
};
#endif

}  // namespace

AidlIntTransactionResult AidlIntTransactionRunner::Run(
    AidlIntTransactionDriver& driver, const AidlIntTransaction& transaction) noexcept {
    if (!IsValidTransaction(transaction)) {
        return Failed(AidlIntTransactionStatus::kInvalidInput);
    }
    if (!driver.PrepareService(transaction.service_name)) {
        return Failed(AidlIntTransactionStatus::kServiceUnavailable);
    }
    if (!driver.ObtainParcels()) {
        driver.Recycle();
        return Failed(AidlIntTransactionStatus::kParcelUnavailable);
    }
    if (!driver.WriteInterfaceToken(transaction.descriptor)) {
        return FailAfterRecycle(driver, AidlIntTransactionStatus::kWriteFailed);
    }
    for (std::size_t index = 0; index < transaction.argument_count; ++index) {
        if (!driver.WriteInt(transaction.arguments[index])) {
            return FailAfterRecycle(driver, AidlIntTransactionStatus::kWriteFailed);
        }
    }
    if (!driver.Transact(transaction.code)) {
        return FailAfterRecycle(driver, AidlIntTransactionStatus::kTransactFailed);
    }
    if (!driver.ReadException()) {
        return FailAfterRecycle(driver, AidlIntTransactionStatus::kRemoteException);
    }

    int reply_value = 0;
    if (!driver.ReadInt(reply_value)) {
        return FailAfterRecycle(driver, AidlIntTransactionStatus::kReadFailed);
    }

    driver.Recycle();
    return AidlIntTransactionResult{
        .ok = true,
        .reply_value = reply_value,
        .status = AidlIntTransactionStatus::kOk,
    };
}

JniAidlIntTransactionBackend::JniAidlIntTransactionBackend(JNIEnv* env) noexcept : env_(env) {}

AidlIntTransactionResult JniAidlIntTransactionBackend::Transact(
    const AidlIntTransaction& transaction) noexcept {
#if defined(__ANDROID__)
    JniAidlIntTransactionDriver driver(env_);
    return AidlIntTransactionRunner::Run(driver, transaction);
#else
    (void)transaction;
    return Failed(AidlIntTransactionStatus::kServiceUnavailable);
#endif
}

}  // namespace fod::vendor
