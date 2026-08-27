#pragma once

#include "fod/jni/jni_env.hpp"
#include "fod/jni/object_description.hpp"

namespace fod::fingerprint {

/**
 * 2026年08月27日23时04分
 * 指纹服务反射访问层。
 *
 * 从 IFingerprintService 走到 BiometricScheduler 需要连续七步字段与集合访问：
 *   service -> this$0 -> mRegistry -> mServiceProviders -> [0]
 *           -> mFingerprintSensors -> valueAt(0) -> mScheduler
 * 移植前这条链连同全部框架类签名，在 HBM 监控、清理看门狗、挑战看门狗三处各写了一遍。
 * 系统一旦改动内部结构就要同步修改三处，极易漏改。这里集中一次实现。
 *
 * 所有返回 jobject 的方法给出的都是局部引用，调用方负责释放，推荐配合 jni::LocalRefScope。
 * 2026 BY white
 */
class ServiceAccess final {
public:
    ServiceAccess(JNIEnv* env, jobject preferred_loader) noexcept;

    [[nodiscard]] JNIEnv* env() const noexcept { return env_; }

    /**
     * 2026年08月27日23时04分
     * 通过 ServiceManager 取得 IFingerprintService 实例。
     * 系统服务发布较晚，启动早期返回空属正常情况，调用方应当重试而非判定失败。
     * 2026 BY white
     */
    [[nodiscard]] jobject ResolveService() noexcept;

    /**
     * 2026年08月27日23时04分
     * 从服务对象走完整条反射链取得 BiometricScheduler。
     * 中间产生的引用在本方法内部全部释放，只把最终的调度器交给调用方。
     * 2026 BY white
     */
    [[nodiscard]] jobject ResolveScheduler(jobject service) noexcept;

    [[nodiscard]] jobject ResolveCurrentOperation(jobject scheduler) noexcept;
    [[nodiscard]] jobject ResolvePendingOperations(jobject scheduler) noexcept;
    [[nodiscard]] jobject ResolveHandler(jobject scheduler) noexcept;
    [[nodiscard]] jobject ResolveCurrentClient(jobject operation) noexcept;

    /**
     * 2026年08月27日23时04分
     * 取得调度器的内部回调对象，取消当前任务时必须原样传回给框架。
     * 2026 BY white
     */
    [[nodiscard]] jobject ResolveInternalCallback(jobject scheduler) noexcept;

    /**
     * 2026年08月27日23时04分
     * 读取当前任务的状态值。部分系统版本没有该字段，此时返回 false 由调用方回退到
     * 从 toString 文本中识别状态。
     * 2026 BY white
     */
    [[nodiscard]] bool ReadOperationState(jobject operation, int& state) noexcept;

    [[nodiscard]] jni::ObjectDescription Describe(jobject object) noexcept;

    void Release(jobject reference) noexcept;

private:
    JNIEnv* env_{};
    jobject preferred_loader_{};
};

}  // namespace fod::fingerprint
