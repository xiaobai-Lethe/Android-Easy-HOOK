#pragma once

#if defined(__ANDROID__)
#include <jni.h>
#else
using jint = int;
struct JNIEnv;
struct JavaVM;
using jobject = void*;
using jclass = void*;
using jobjectArray = void*;
struct JNINativeMethod {
    char* name;
    char* signature;
    void* fnPtr;
};
#endif

namespace fod::jni {

// 为原生后台线程取得 JNIEnv；仅在本对象执行 attach 时才会在析构中 detach。
class ScopedThreadEnv final {
public:
    explicit ScopedThreadEnv(JavaVM* vm) noexcept;
    ~ScopedThreadEnv() noexcept;

    ScopedThreadEnv(const ScopedThreadEnv&) = delete;
    ScopedThreadEnv& operator=(const ScopedThreadEnv&) = delete;

    [[nodiscard]] JNIEnv* get() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

private:
    JavaVM* vm_{};
    JNIEnv* env_{};
    // 防止把原本就由 ART 管理的线程错误地 detach。
    bool attached_thread_{};
};

}  // namespace fod::jni
