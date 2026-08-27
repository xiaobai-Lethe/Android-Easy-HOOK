#include "fod/jni/service_resolver.hpp"

#include "fod/jni/class_resolver.hpp"

#include <cctype>
#include <string>

namespace fod::jni {
namespace {

constexpr char kServiceManagerClass[] = "android/os/ServiceManager";
constexpr char kGetServiceMethod[] = "getService";
constexpr char kGetServiceSignature[] = "(Ljava/lang/String;)Landroid/os/IBinder;";
constexpr char kAsInterfaceMethod[] = "asInterface";
constexpr char kFingerprintAsInterfaceSignature[] =
    "(Landroid/os/IBinder;)Landroid/hardware/fingerprint/IFingerprintService;";

#if defined(__ANDROID__)
void ClearJniException(JNIEnv* env) noexcept {
    if (env != nullptr && env->ExceptionCheck() == JNI_TRUE) {
        env->ExceptionClear();
    }
}
#endif

}  // namespace

ServiceResolver::ServiceResolver(JNIEnv* env, jobject preferred_loader) noexcept
    : env_(env), preferred_loader_(preferred_loader) {}

jobject ServiceResolver::GetService(std::string_view service_name) const noexcept {
    if (env_ == nullptr || !IsValidServiceName(service_name)) {
        return nullptr;
    }

#if defined(__ANDROID__)
    jclass service_manager_class = env_->FindClass(kServiceManagerClass);
    ClearJniException(env_);
    if (service_manager_class == nullptr) {
        return nullptr;
    }

    jmethodID get_service =
        env_->GetStaticMethodID(service_manager_class, kGetServiceMethod, kGetServiceSignature);
    ClearJniException(env_);
    if (get_service == nullptr) {
        env_->DeleteLocalRef(service_manager_class);
        return nullptr;
    }

    std::string service_name_string;
    try {
        service_name_string.assign(service_name);
    } catch (...) {
        env_->DeleteLocalRef(service_manager_class);
        return nullptr;
    }

    jstring service_name_object = env_->NewStringUTF(service_name_string.c_str());
    ClearJniException(env_);
    if (service_name_object == nullptr) {
        env_->DeleteLocalRef(service_manager_class);
        return nullptr;
    }

    jobject binder =
        env_->CallStaticObjectMethod(service_manager_class, get_service, service_name_object);
    env_->DeleteLocalRef(service_name_object);
    env_->DeleteLocalRef(service_manager_class);
    ClearJniException(env_);
    return binder;
#else
    (void)service_name;
    return nullptr;
#endif
}

jobject ServiceResolver::GetFingerprintService() const noexcept {
#if defined(__ANDROID__)
    jobject binder = GetService(kFingerprintServiceName);
    if (binder == nullptr) {
        return nullptr;
    }

    ClassResolver resolver(env_, preferred_loader_);
    jclass stub_class = resolver.Resolve(kFingerprintServiceStubClass);
    if (stub_class == nullptr) {
        env_->DeleteLocalRef(binder);
        return nullptr;
    }

    jmethodID as_interface =
        env_->GetStaticMethodID(stub_class, kAsInterfaceMethod, kFingerprintAsInterfaceSignature);
    ClearJniException(env_);
    if (as_interface == nullptr) {
        env_->DeleteLocalRef(stub_class);
        env_->DeleteLocalRef(binder);
        return nullptr;
    }

    jobject service = env_->CallStaticObjectMethod(stub_class, as_interface, binder);
    env_->DeleteLocalRef(stub_class);
    env_->DeleteLocalRef(binder);
    ClearJniException(env_);
    return service;
#else
    return nullptr;
#endif
}

bool ServiceResolver::IsValidServiceName(std::string_view service_name) noexcept {
    if (service_name.empty()) {
        return false;
    }

    for (char character : service_name) {
        const auto byte = static_cast<unsigned char>(character);
        if (character == '\0' || std::iscntrl(byte) != 0 || std::isspace(byte) != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace fod::jni
