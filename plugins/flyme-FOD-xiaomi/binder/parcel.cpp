#include "flyme-FOD-xiaomi/binder/parcel.hpp"

#include <atomic>
#include <utility>

#if defined(__ANDROID__)
#include <jni.h>
#endif

namespace fod::binder {
namespace {

std::string TruncateDescriptor(std::string value) {
    if (value.size() > ParcelDescriptorReader::kMaxDescriptorLength) {
        value.resize(ParcelDescriptorReader::kMaxDescriptorLength);
    }
    return value;
}

#if defined(__ANDROID__)
void ClearJniException(JNIEnv* env) noexcept {
    if (env != nullptr && env->ExceptionCheck() == JNI_TRUE) {
        env->ExceptionClear();
    }
}

/**
 * android.os.Parcel 由 boot classloader 加载，类与方法 ID 全进程稳定，
 * 解析一次即可全程复用。system_server 上每个候选事务都重解一遍纯属浪费，
 * 缓存后省去每次的 GetObjectClass 与三次 GetMethodID。
 */
struct ParcelMethodIds final {
    std::atomic<jmethodID> data_position{nullptr};
    std::atomic<jmethodID> set_data_position{nullptr};
    std::atomic<jmethodID> read_string{nullptr};
    std::atomic<bool> resolved{false};
};

ParcelMethodIds g_parcel_method_ids;

void ResolveParcelMethodIds(JNIEnv* env, jobject parcel) noexcept {
    jclass parcel_class = env->GetObjectClass(parcel);
    ClearJniException(env);
    if (parcel_class == nullptr) {
        return;
    }

    const jmethodID data_position = env->GetMethodID(parcel_class, "dataPosition", "()I");
    ClearJniException(env);
    const jmethodID set_data_position = env->GetMethodID(parcel_class, "setDataPosition", "(I)V");
    ClearJniException(env);
    const jmethodID read_string =
        env->GetMethodID(parcel_class, "readString", "()Ljava/lang/String;");
    ClearJniException(env);
    env->DeleteLocalRef(parcel_class);

    if (data_position == nullptr || set_data_position == nullptr || read_string == nullptr) {
        return;
    }

    g_parcel_method_ids.data_position.store(data_position, std::memory_order_relaxed);
    g_parcel_method_ids.set_data_position.store(set_data_position, std::memory_order_relaxed);
    g_parcel_method_ids.read_string.store(read_string, std::memory_order_relaxed);
    g_parcel_method_ids.resolved.store(true, std::memory_order_release);
}

class JniParcelDescriptorBackend final : public ParcelDescriptorBackend {
public:
    JniParcelDescriptorBackend(JNIEnv* env, jobject parcel) noexcept
        : env_(env), parcel_(parcel) {
        if (env_ == nullptr || parcel_ == nullptr) {
            return;
        }

        if (!g_parcel_method_ids.resolved.load(std::memory_order_acquire)) {
            ResolveParcelMethodIds(env_, parcel_);
        }
        data_position_ = g_parcel_method_ids.data_position.load(std::memory_order_relaxed);
        set_data_position_ = g_parcel_method_ids.set_data_position.load(std::memory_order_relaxed);
        read_string_ = g_parcel_method_ids.read_string.load(std::memory_order_relaxed);
    }

    bool ReadDataPosition(int& position) noexcept override {
        if (env_ == nullptr || parcel_ == nullptr || data_position_ == nullptr) {
            return false;
        }

        const jint current_position = env_->CallIntMethod(parcel_, data_position_);
        if (env_->ExceptionCheck() == JNI_TRUE) {
            ClearJniException(env_);
            return false;
        }

        position = static_cast<int>(current_position);
        return true;
    }

    bool SetDataPosition(int position) noexcept override {
        if (env_ == nullptr || parcel_ == nullptr || set_data_position_ == nullptr) {
            return false;
        }

        env_->CallVoidMethod(parcel_, set_data_position_, static_cast<jint>(position));
        if (env_->ExceptionCheck() == JNI_TRUE) {
            ClearJniException(env_);
            return false;
        }
        return true;
    }

    bool ReadString(std::string& value) noexcept override {
        if (env_ == nullptr || parcel_ == nullptr || read_string_ == nullptr) {
            return false;
        }

        auto descriptor = static_cast<jstring>(env_->CallObjectMethod(parcel_, read_string_));
        if (env_->ExceptionCheck() == JNI_TRUE) {
            ClearJniException(env_);
            return false;
        }
        if (descriptor == nullptr) {
            return false;
        }

        const char* utf_descriptor = env_->GetStringUTFChars(descriptor, nullptr);
        if (env_->ExceptionCheck() == JNI_TRUE || utf_descriptor == nullptr) {
            ClearJniException(env_);
            env_->DeleteLocalRef(descriptor);
            return false;
        }

        bool copied = true;
        try {
            value.assign(utf_descriptor);
        } catch (...) {
            copied = false;
        }

        env_->ReleaseStringUTFChars(descriptor, utf_descriptor);
        env_->DeleteLocalRef(descriptor);
        ClearJniException(env_);
        return copied;
    }

private:
    JNIEnv* env_{};
    jobject parcel_{};
    jmethodID data_position_{};
    jmethodID set_data_position_{};
    jmethodID read_string_{};
};
#endif

}  // namespace

ParcelDescriptorResult ParcelDescriptorReader::ReadInterfaceDescriptor(
    ParcelDescriptorBackend& backend) {
    ParcelDescriptorResult result;

    int original_position = 0;
    if (!backend.ReadDataPosition(original_position)) {
        result.status = ParcelDescriptorStatus::kDataPositionFailed;
        return result;
    }

    ParcelDescriptorStatus status = ParcelDescriptorStatus::kRead;
    if (!backend.SetDataPosition(0)) {
        status = ParcelDescriptorStatus::kSetDataPositionFailed;
    } else {
        std::string descriptor;
        if (backend.ReadString(descriptor)) {
            result.descriptor = TruncateDescriptor(std::move(descriptor));
        } else {
            status = ParcelDescriptorStatus::kReadStringFailed;
        }
    }

    result.restored_position = backend.SetDataPosition(original_position);
    result.status =
        result.restored_position ? status : ParcelDescriptorStatus::kRestorePositionFailed;
    return result;
}

ParcelDescriptorResult ReadParcelInterfaceDescriptor(JNIEnv* env, jobject parcel) noexcept {
#if defined(__ANDROID__)
    if (env == nullptr || parcel == nullptr) {
        ParcelDescriptorResult result;
        result.status = ParcelDescriptorStatus::kMissingParcel;
        return result;
    }

    JniParcelDescriptorBackend backend(env, parcel);
    try {
        return ParcelDescriptorReader::ReadInterfaceDescriptor(backend);
    } catch (...) {
        ParcelDescriptorResult result;
        result.status = ParcelDescriptorStatus::kReadStringFailed;
        return result;
    }
#else
    (void)env;
    (void)parcel;
    ParcelDescriptorResult result;
    result.status = ParcelDescriptorStatus::kMissingParcel;
    return result;
#endif
}

}  // namespace fod::binder
