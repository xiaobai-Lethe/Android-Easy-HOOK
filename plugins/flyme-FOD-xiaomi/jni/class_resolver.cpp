#include "flyme-FOD-xiaomi/jni/class_resolver.hpp"

#include <algorithm>
#include <cctype>

namespace fod::jni {
namespace {

std::string ReplaceSeparator(std::string_view value, char from, char to) {
    std::string result(value);
    std::replace(result.begin(), result.end(), from, to);
    return result;
}

bool IsNameCharacter(char value) noexcept {
    const auto byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_' || value == '$';
}

#if defined(__ANDROID__)
void ClearException(JNIEnv* env) noexcept {
    if (env != nullptr && env->ExceptionCheck() == JNI_TRUE) {
        env->ExceptionClear();
    }
}

jclass LoadClassWithLoader(JNIEnv* env, jobject loader, const std::string& dot_name) noexcept {
    if (env == nullptr || loader == nullptr) {
        return nullptr;
    }

    jclass loader_class = env->FindClass("java/lang/ClassLoader");
    ClearException(env);
    if (loader_class == nullptr) {
        return nullptr;
    }

    jmethodID load_class = env->GetMethodID(loader_class, "loadClass",
                                           "(Ljava/lang/String;)Ljava/lang/Class;");
    env->DeleteLocalRef(loader_class);
    ClearException(env);
    if (load_class == nullptr) {
        return nullptr;
    }

    jstring class_name = env->NewStringUTF(dot_name.c_str());
    if (class_name == nullptr) {
        ClearException(env);
        return nullptr;
    }

    auto resolved = static_cast<jclass>(env->CallObjectMethod(loader, load_class, class_name));
    env->DeleteLocalRef(class_name);
    ClearException(env);
    return resolved;
}

jobject CurrentThreadContextLoader(JNIEnv* env) noexcept {
    jclass thread_class = env->FindClass("java/lang/Thread");
    ClearException(env);
    if (thread_class == nullptr) {
        return nullptr;
    }

    jmethodID current_thread = env->GetStaticMethodID(thread_class, "currentThread",
                                                     "()Ljava/lang/Thread;");
    jmethodID get_context_loader = env->GetMethodID(thread_class, "getContextClassLoader",
                                                   "()Ljava/lang/ClassLoader;");
    if (current_thread == nullptr || get_context_loader == nullptr) {
        env->DeleteLocalRef(thread_class);
        ClearException(env);
        return nullptr;
    }

    jobject thread = env->CallStaticObjectMethod(thread_class, current_thread);
    ClearException(env);
    jobject loader = nullptr;
    if (thread != nullptr) {
        loader = env->CallObjectMethod(thread, get_context_loader);
        env->DeleteLocalRef(thread);
        ClearException(env);
    }
    env->DeleteLocalRef(thread_class);
    return loader;
}

jobject SystemClassLoader(JNIEnv* env) noexcept {
    jclass loader_class = env->FindClass("java/lang/ClassLoader");
    ClearException(env);
    if (loader_class == nullptr) {
        return nullptr;
    }

    jmethodID get_system_loader = env->GetStaticMethodID(loader_class, "getSystemClassLoader",
                                                        "()Ljava/lang/ClassLoader;");
    if (get_system_loader == nullptr) {
        env->DeleteLocalRef(loader_class);
        ClearException(env);
        return nullptr;
    }

    jobject loader = env->CallStaticObjectMethod(loader_class, get_system_loader);
    env->DeleteLocalRef(loader_class);
    ClearException(env);
    return loader;
}

jclass LoadFromLiveThreadLoaders(JNIEnv* env, const std::string& dot_name) noexcept {
    jclass thread_class = env->FindClass("java/lang/Thread");
    ClearException(env);
    if (thread_class == nullptr) {
        return nullptr;
    }

    jmethodID get_all_stack_traces = env->GetStaticMethodID(
        thread_class, "getAllStackTraces", "()Ljava/util/Map;");
    jmethodID get_context_loader = env->GetMethodID(thread_class, "getContextClassLoader",
                                                   "()Ljava/lang/ClassLoader;");
    if (get_all_stack_traces == nullptr || get_context_loader == nullptr) {
        env->DeleteLocalRef(thread_class);
        ClearException(env);
        return nullptr;
    }

    jobject traces = env->CallStaticObjectMethod(thread_class, get_all_stack_traces);
    ClearException(env);
    if (traces == nullptr) {
        env->DeleteLocalRef(thread_class);
        return nullptr;
    }

    jclass map_class = env->FindClass("java/util/Map");
    jclass set_class = env->FindClass("java/util/Set");
    jclass object_array_class = env->FindClass("[Ljava/lang/Object;");
    ClearException(env);
    if (map_class == nullptr || set_class == nullptr || object_array_class == nullptr) {
        if (map_class != nullptr) {
            env->DeleteLocalRef(map_class);
        }
        if (set_class != nullptr) {
            env->DeleteLocalRef(set_class);
        }
        if (object_array_class != nullptr) {
            env->DeleteLocalRef(object_array_class);
        }
        env->DeleteLocalRef(traces);
        env->DeleteLocalRef(thread_class);
        return nullptr;
    }

    jmethodID key_set = env->GetMethodID(map_class, "keySet", "()Ljava/util/Set;");
    jmethodID to_array = env->GetMethodID(set_class, "toArray", "()[Ljava/lang/Object;");
    env->DeleteLocalRef(map_class);
    env->DeleteLocalRef(set_class);
    env->DeleteLocalRef(object_array_class);
    ClearException(env);
    if (key_set == nullptr || to_array == nullptr) {
        env->DeleteLocalRef(traces);
        env->DeleteLocalRef(thread_class);
        return nullptr;
    }

    jobject threads_set = env->CallObjectMethod(traces, key_set);
    env->DeleteLocalRef(traces);
    ClearException(env);
    if (threads_set == nullptr) {
        env->DeleteLocalRef(thread_class);
        return nullptr;
    }

    auto threads = static_cast<jobjectArray>(env->CallObjectMethod(threads_set, to_array));
    env->DeleteLocalRef(threads_set);
    ClearException(env);
    if (threads == nullptr) {
        env->DeleteLocalRef(thread_class);
        return nullptr;
    }

    const jsize thread_count = env->GetArrayLength(threads);
    const auto bounded_count =
        std::min<std::size_t>(static_cast<std::size_t>(thread_count),
                              ClassResolver::kMaxLiveThreadLoaders);
    for (std::size_t index = 0; index < bounded_count; ++index) {
        jobject thread = env->GetObjectArrayElement(threads, static_cast<jsize>(index));
        ClearException(env);
        if (thread == nullptr) {
            continue;
        }

        jobject loader = env->CallObjectMethod(thread, get_context_loader);
        env->DeleteLocalRef(thread);
        ClearException(env);
        if (loader == nullptr) {
            continue;
        }

        jclass resolved = LoadClassWithLoader(env, loader, dot_name);
        env->DeleteLocalRef(loader);
        if (resolved != nullptr) {
            env->DeleteLocalRef(threads);
            env->DeleteLocalRef(thread_class);
            return resolved;
        }
    }

    env->DeleteLocalRef(threads);
    env->DeleteLocalRef(thread_class);
    return nullptr;
}
#endif

}  // namespace

ClassResolver::ClassResolver(JNIEnv* env, jobject preferred_loader) noexcept
    : env_(env), preferred_loader_(preferred_loader) {}

jclass ClassResolver::Resolve(std::string_view binary_name) const noexcept {
#if defined(__ANDROID__)
    if (env_ == nullptr || !IsValidBinaryName(binary_name)) {
        return nullptr;
    }

    const std::string slash_name = ToSlashName(binary_name);
    jclass resolved = env_->FindClass(slash_name.c_str());
    ClearException(env_);
    if (resolved != nullptr) {
        return resolved;
    }

    const std::string dot_name = ToDotName(binary_name);
    resolved = LoadClassWithLoader(env_, preferred_loader_, dot_name);
    if (resolved != nullptr) {
        return resolved;
    }

    jobject context_loader = CurrentThreadContextLoader(env_);
    resolved = LoadClassWithLoader(env_, context_loader, dot_name);
    if (context_loader != nullptr) {
        env_->DeleteLocalRef(context_loader);
    }
    if (resolved != nullptr) {
        return resolved;
    }

    jobject system_loader = SystemClassLoader(env_);
    resolved = LoadClassWithLoader(env_, system_loader, dot_name);
    if (system_loader != nullptr) {
        env_->DeleteLocalRef(system_loader);
    }
    if (resolved != nullptr) {
        return resolved;
    }

    return LoadFromLiveThreadLoaders(env_, dot_name);
#else
    (void)binary_name;
    return nullptr;
#endif
}

std::string ClassResolver::ToDotName(std::string_view binary_name) {
    return ReplaceSeparator(binary_name, '/', '.');
}

std::string ClassResolver::ToSlashName(std::string_view binary_name) {
    return ReplaceSeparator(binary_name, '.', '/');
}

bool ClassResolver::IsValidBinaryName(std::string_view binary_name) noexcept {
    if (binary_name.empty()) {
        return false;
    }

    const bool uses_dot = binary_name.find('.') != std::string_view::npos;
    const bool uses_slash = binary_name.find('/') != std::string_view::npos;
    if (uses_dot && uses_slash) {
        return false;
    }

    bool previous_was_separator = true;
    for (char character : binary_name) {
        const bool is_separator = character == '.' || character == '/';
        if (is_separator) {
            if (previous_was_separator) {
                return false;
            }
            previous_was_separator = true;
            continue;
        }

        if (!IsNameCharacter(character)) {
            return false;
        }
        previous_was_separator = false;
    }

    return !previous_was_separator;
}

}  // namespace fod::jni
