#include "flyme-FOD-xiaomi/jni/jni_env.hpp"

namespace fod::jni {

ScopedThreadEnv::ScopedThreadEnv(JavaVM* vm) noexcept : vm_(vm) {
#if defined(__ANDROID__)
    if (vm_ == nullptr) {
        return;
    }

    void* raw_env = nullptr;
    const jint get_env_result = vm_->GetEnv(&raw_env, JNI_VERSION_1_6);
    if (get_env_result == JNI_OK) {
        env_ = static_cast<JNIEnv*>(raw_env);
        return;
    }
    if (get_env_result != JNI_EDETACHED) {
        return;
    }

    JNIEnv* attached_env = nullptr;
    if (vm_->AttachCurrentThread(&attached_env, nullptr) == JNI_OK) {
        env_ = attached_env;
        attached_thread_ = true;
    }
#else
    (void)vm_;
#endif
}

ScopedThreadEnv::~ScopedThreadEnv() noexcept {
#if defined(__ANDROID__)
    if (vm_ != nullptr && attached_thread_) {
        vm_->DetachCurrentThread();
    }
#endif
}

JNIEnv* ScopedThreadEnv::get() const noexcept {
    return env_;
}

ScopedThreadEnv::operator bool() const noexcept {
    return env_ != nullptr;
}

}  // namespace fod::jni
