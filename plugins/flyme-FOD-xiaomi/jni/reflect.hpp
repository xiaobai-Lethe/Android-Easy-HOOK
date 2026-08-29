#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"
#include "flyme-FOD-xiaomi/jni/object_description.hpp"

#include <array>
#include <cstddef>

namespace fod::jni {

/**
 * 2026年08月27日22时58分
 * JNI 反射公共工具。移植前这组函数在三个监控模块和两个补丁模块中各存在一份完全相同的副本，
 * 任何一处修正都必须同步五遍。这里集中一次实现，其余模块统一调用。
 * 全部函数在失败时清理挂起异常并返回失败值，绝不把异常留给调用方或抛出。
 * 2026 BY white
 */

/**
 * 2026年08月27日22时58分
 * 清理挂起的 JNI 异常，并返回本次是否确实存在异常。
 * 反射访问系统内部字段时找不到成员是常态，必须逐次清理，否则后续 JNI 调用都会失败。
 * 2026 BY white
 */
bool ConsumeException(JNIEnv* env) noexcept;

void ClearException(JNIEnv* env) noexcept;

/**
 * 2026年08月27日22时58分
 * 沿类继承链逐级查找字段。系统实现常把字段定义在父类中，只查对象自身的类会漏掉。
 * 2026 BY white
 */
[[nodiscard]] jfieldID FindFieldInHierarchy(JNIEnv* env, jobject object, const char* name,
                                            const char* signature) noexcept;

[[nodiscard]] jfieldID FindFieldInHierarchy(JNIEnv* env, jclass clazz, const char* name,
                                            const char* signature) noexcept;

[[nodiscard]] jobject ReadObjectField(JNIEnv* env, jobject object, const char* name,
                                      const char* signature) noexcept;

[[nodiscard]] bool ReadIntField(JNIEnv* env, jobject object, const char* name,
                                int& value) noexcept;

[[nodiscard]] bool ReadBooleanField(JNIEnv* env, jobject object, const char* name,
                                    bool& value) noexcept;

[[nodiscard]] bool WriteIntField(JNIEnv* env, jobject object, const char* name,
                                 int value) noexcept;

[[nodiscard]] bool WriteBooleanField(JNIEnv* env, jobject object, const char* name,
                                     bool value) noexcept;

[[nodiscard]] bool WriteObjectField(JNIEnv* env, jobject object, const char* name,
                                    const char* signature, jobject value) noexcept;

/**
 * 2026年08月27日22时58分
 * 调用形如 get(int) 或 valueAt(int) 的方法取出集合元素。
 * List 与 SparseArray 的取值方法名不同，因此方法名由调用方给出。
 * 2026 BY white
 */
[[nodiscard]] jobject ReadIndexedObject(JNIEnv* env, jobject collection, const char* method_name,
                                        int index) noexcept;

[[nodiscard]] bool ReadCollectionSize(JNIEnv* env, jobject collection, const char* method_name,
                                      int& size) noexcept;

/**
 * 2026年08月27日22时58分
 * 调用对象的 toString() 并取回文本。
 * 指纹调度器把当前任务的类型、归属和状态都写在 toString 里，是判断运行状态的主要依据。
 * 2026 BY white
 */
[[nodiscard]] ObjectDescription DescribeObject(JNIEnv* env, jobject object) noexcept;

/**
 * 2026年08月27日22时58分
 * 局部引用作用域。反射链上每一步都会产生局部引用，
 * 监控线程每秒运行一次，漏删会迅速耗尽局部引用表并让整个进程的 JNI 调用失败。
 * 析构时按加入的相反顺序释放。
 * 2026 BY white
 */
class LocalRefScope final {
public:
    static constexpr std::size_t kCapacity = 8;

    explicit LocalRefScope(JNIEnv* env) noexcept : env_(env) {}
    ~LocalRefScope();

    LocalRefScope(const LocalRefScope&) = delete;
    LocalRefScope& operator=(const LocalRefScope&) = delete;

    /**
     * 2026年08月27日22时58分
     * 纳入一个局部引用并原样返回，便于在表达式中直接串联反射调用。
     * 超出容量时不再记录，此时由调用方自行释放。
     * 2026 BY white
     */
    jobject Track(jobject reference) noexcept;

    void Release(jobject reference) noexcept;

private:
    JNIEnv* env_{};
    std::array<jobject, kCapacity> references_{};
    std::size_t count_{};
};

}  // namespace fod::jni
