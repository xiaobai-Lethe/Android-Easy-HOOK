#include "flyme-FOD-xiaomi/jni/reflect.hpp"

#include <string>
#include <utility>

namespace fod::jni {
namespace {

#if defined(__ANDROID__)

/**
 * 2026年08月27日23时00分
 * 取得对象所属类，并把它纳入调用方的引用作用域。
 * 2026 BY white
 */
jclass ObjectClass(JNIEnv* env, jobject object) noexcept {
    if (env == nullptr || object == nullptr) {
        return nullptr;
    }
    jclass clazz = env->GetObjectClass(object);
    if (ConsumeException(env)) {
        return nullptr;
    }
    return clazz;
}

#endif

}  // namespace

bool ConsumeException(JNIEnv* env) noexcept {
#if defined(__ANDROID__)
    if (env == nullptr || env->ExceptionCheck() != JNI_TRUE) {
        return false;
    }
    env->ExceptionClear();
    return true;
#else
    (void)env;
    return false;
#endif
}

void ClearException(JNIEnv* env) noexcept {
    static_cast<void>(ConsumeException(env));
}

jfieldID FindFieldInHierarchy(JNIEnv* env, jclass clazz, const char* name,
                              const char* signature) noexcept {
#if defined(__ANDROID__)
    if (env == nullptr || clazz == nullptr || name == nullptr || signature == nullptr) {
        return nullptr;
    }

    /**
     * 2026年08月27日23时00分
     * current 起始为调用方持有的类引用，本函数不应释放它；
     * 只有向上遍历过程中自己取得的父类引用才由本函数释放。
     * 2026 BY white
     */
    jclass current = clazz;
    bool owns_current = false;
    while (current != nullptr) {
        jfieldID field = env->GetFieldID(current, name, signature);
        if (!ConsumeException(env) && field != nullptr) {
            if (owns_current) {
                env->DeleteLocalRef(current);
            }
            return field;
        }

        jclass parent = env->GetSuperclass(current);
        ConsumeException(env);
        if (owns_current) {
            env->DeleteLocalRef(current);
        }
        current = parent;
        owns_current = true;
    }
    return nullptr;
#else
    (void)env;
    (void)clazz;
    (void)name;
    (void)signature;
    return nullptr;
#endif
}

jfieldID FindFieldInHierarchy(JNIEnv* env, jobject object, const char* name,
                              const char* signature) noexcept {
#if defined(__ANDROID__)
    jclass clazz = ObjectClass(env, object);
    if (clazz == nullptr) {
        return nullptr;
    }
    jfieldID field = FindFieldInHierarchy(env, clazz, name, signature);
    env->DeleteLocalRef(clazz);
    return field;
#else
    (void)env;
    (void)object;
    (void)name;
    (void)signature;
    return nullptr;
#endif
}

jobject ReadObjectField(JNIEnv* env, jobject object, const char* name,
                        const char* signature) noexcept {
#if defined(__ANDROID__)
    const jfieldID field = FindFieldInHierarchy(env, object, name, signature);
    if (field == nullptr) {
        return nullptr;
    }

    jobject value = env->GetObjectField(object, field);
    if (ConsumeException(env)) {
        if (value != nullptr) {
            env->DeleteLocalRef(value);
        }
        return nullptr;
    }
    return value;
#else
    (void)env;
    (void)object;
    (void)name;
    (void)signature;
    return nullptr;
#endif
}

bool ReadIntField(JNIEnv* env, jobject object, const char* name, int& value) noexcept {
#if defined(__ANDROID__)
    const jfieldID field = FindFieldInHierarchy(env, object, name, "I");
    if (field == nullptr) {
        return false;
    }
    value = static_cast<int>(env->GetIntField(object, field));
    return !ConsumeException(env);
#else
    (void)env;
    (void)object;
    (void)name;
    (void)value;
    return false;
#endif
}

bool ReadBooleanField(JNIEnv* env, jobject object, const char* name, bool& value) noexcept {
#if defined(__ANDROID__)
    const jfieldID field = FindFieldInHierarchy(env, object, name, "Z");
    if (field == nullptr) {
        return false;
    }
    value = env->GetBooleanField(object, field) == JNI_TRUE;
    return !ConsumeException(env);
#else
    (void)env;
    (void)object;
    (void)name;
    (void)value;
    return false;
#endif
}

bool WriteIntField(JNIEnv* env, jobject object, const char* name, int value) noexcept {
#if defined(__ANDROID__)
    const jfieldID field = FindFieldInHierarchy(env, object, name, "I");
    if (field == nullptr) {
        return false;
    }
    env->SetIntField(object, field, static_cast<jint>(value));
    return !ConsumeException(env);
#else
    (void)env;
    (void)object;
    (void)name;
    (void)value;
    return false;
#endif
}

bool WriteBooleanField(JNIEnv* env, jobject object, const char* name, bool value) noexcept {
#if defined(__ANDROID__)
    const jfieldID field = FindFieldInHierarchy(env, object, name, "Z");
    if (field == nullptr) {
        return false;
    }
    env->SetBooleanField(object, field, value ? JNI_TRUE : JNI_FALSE);
    return !ConsumeException(env);
#else
    (void)env;
    (void)object;
    (void)name;
    (void)value;
    return false;
#endif
}

bool WriteObjectField(JNIEnv* env, jobject object, const char* name, const char* signature,
                      jobject value) noexcept {
#if defined(__ANDROID__)
    const jfieldID field = FindFieldInHierarchy(env, object, name, signature);
    if (field == nullptr) {
        return false;
    }
    env->SetObjectField(object, field, value);
    return !ConsumeException(env);
#else
    (void)env;
    (void)object;
    (void)name;
    (void)signature;
    (void)value;
    return false;
#endif
}

jobject ReadIndexedObject(JNIEnv* env, jobject collection, const char* method_name,
                          int index) noexcept {
#if defined(__ANDROID__)
    if (env == nullptr || collection == nullptr || method_name == nullptr || index < 0) {
        return nullptr;
    }

    jclass collection_class = ObjectClass(env, collection);
    if (collection_class == nullptr) {
        return nullptr;
    }
    const jmethodID method =
        env->GetMethodID(collection_class, method_name, "(I)Ljava/lang/Object;");
    ConsumeException(env);
    env->DeleteLocalRef(collection_class);
    if (method == nullptr) {
        return nullptr;
    }

    jobject value = env->CallObjectMethod(collection, method, static_cast<jint>(index));
    if (ConsumeException(env)) {
        if (value != nullptr) {
            env->DeleteLocalRef(value);
        }
        return nullptr;
    }
    return value;
#else
    (void)env;
    (void)collection;
    (void)method_name;
    (void)index;
    return nullptr;
#endif
}

bool ReadCollectionSize(JNIEnv* env, jobject collection, const char* method_name,
                        int& size) noexcept {
#if defined(__ANDROID__)
    if (env == nullptr || collection == nullptr || method_name == nullptr) {
        return false;
    }

    jclass collection_class = ObjectClass(env, collection);
    if (collection_class == nullptr) {
        return false;
    }
    const jmethodID method = env->GetMethodID(collection_class, method_name, "()I");
    ConsumeException(env);
    env->DeleteLocalRef(collection_class);
    if (method == nullptr) {
        return false;
    }

    const jint value = env->CallIntMethod(collection, method);
    if (ConsumeException(env)) {
        return false;
    }
    size = static_cast<int>(value);
    return size >= 0;
#else
    (void)env;
    (void)collection;
    (void)method_name;
    (void)size;
    return false;
#endif
}

ObjectDescription DescribeObject(JNIEnv* env, jobject object) noexcept {
    if (object == nullptr) {
        return ObjectDescription{.status = ObjectDescriptionStatus::kNull, .text = {}};
    }

#if defined(__ANDROID__)
    const ObjectDescription failure{.status = ObjectDescriptionStatus::kFailed, .text = {}};

    jclass object_class = ObjectClass(env, object);
    if (object_class == nullptr) {
        return failure;
    }
    const jmethodID to_string =
        env->GetMethodID(object_class, "toString", "()Ljava/lang/String;");
    ConsumeException(env);
    env->DeleteLocalRef(object_class);
    if (to_string == nullptr) {
        return failure;
    }

    auto text = static_cast<jstring>(env->CallObjectMethod(object, to_string));
    if (ConsumeException(env) || text == nullptr) {
        if (text != nullptr) {
            env->DeleteLocalRef(text);
        }
        return failure;
    }

    const char* bytes = env->GetStringUTFChars(text, nullptr);
    if (ConsumeException(env) || bytes == nullptr) {
        env->DeleteLocalRef(text);
        return failure;
    }

    ObjectDescription result{.status = ObjectDescriptionStatus::kDescribed, .text = {}};
    try {
        result.text.assign(bytes);
    } catch (...) {
        env->ReleaseStringUTFChars(text, bytes);
        env->DeleteLocalRef(text);
        return failure;
    }

    env->ReleaseStringUTFChars(text, bytes);
    env->DeleteLocalRef(text);
    return result;
#else
    (void)env;
    return ObjectDescription{.status = ObjectDescriptionStatus::kFailed, .text = {}};
#endif
}

LocalRefScope::~LocalRefScope() {
#if defined(__ANDROID__)
    while (count_ > 0) {
        jobject reference = references_[--count_];
        if (env_ != nullptr && reference != nullptr) {
            env_->DeleteLocalRef(reference);
        }
    }
#endif
}

jobject LocalRefScope::Track(jobject reference) noexcept {
    if (reference != nullptr && count_ < references_.size()) {
        references_[count_++] = reference;
    }
    return reference;
}

void LocalRefScope::Release(jobject reference) noexcept {
#if defined(__ANDROID__)
    if (env_ != nullptr && reference != nullptr) {
        env_->DeleteLocalRef(reference);
        ClearException(env_);
    }
#else
    (void)reference;
#endif
}

}  // namespace fod::jni
