#pragma once

#include "flyme-FOD-xiaomi/jni/jni_env.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace fod::monitor {

/**
 * 2026年08月27日23时08分
 * 监控线程的启动结果。区分"按配置跳过"与"真的启动失败"，便于日志如实反映原因。
 * 2026 BY white
 */
enum class MonitorThreadStatus : std::uint8_t {
    kStarted,
    kAlreadyStarted,
    kInvalidConfig,
    kMissingJniEnv,
    kMissingJavaVm,
    kLoaderReferenceFailed,
    kAllocationFailed,
    kThreadStartFailed,
    kUnsupportedPlatform,
};

[[nodiscard]] const char* MonitorThreadStatusName(MonitorThreadStatus status) noexcept;

struct MonitorThreadConfig final {
    std::string_view log_tag;
    int max_attempts{0};  // 0 表示不限次数，随宿主进程长期运行。
    int interval_ms{1000};
};

/**
 * 2026年08月27日23时08分
 * 监控任务接口。使用者只实现每轮要做的判断，线程创建、ART 附着、休眠节奏、
 * 全局引用释放等全部由运行器负责。
 * 2026 BY white
 */
class MonitorTask {
public:
    MonitorTask() = default;
    virtual ~MonitorTask() = default;

    MonitorTask(const MonitorTask&) = delete;
    MonitorTask& operator=(const MonitorTask&) = delete;

    /**
     * 2026年08月27日23时08分
     * 执行一轮检查。返回 false 表示任务已完成，运行器会结束循环。
     * attempt 从 0 开始递增，可用于安排低频的周期性动作。
     * 2026 BY white
     */
    [[nodiscard]] virtual bool Tick(JNIEnv* env, jobject preferred_loader,
                                    int attempt) noexcept = 0;

    /**
     * 2026年08月27日23时08分
     * 循环结束后的收尾。用于把进程状态恢复到未介入前，默认什么都不做。
     * 2026 BY white
     */
    virtual void Finish(JNIEnv* env, jobject preferred_loader) noexcept {
        static_cast<void>(env);
        static_cast<void>(preferred_loader);
    }
};

/**
 * 2026年08月27日23时08分
 * 在独立的分离线程上运行一个监控任务。
 *
 * 移植前三个监控各自复制了一份完全相同的线程启动代码：相位原子量、JavaVM 获取、
 * 类加载器全局引用、线程属性设置、失败时的多级回滚，每处约七十行且逐字雷同。
 * 这里集中一次实现，各监控只需提供 MonitorTask。
 *
 * 线程创建成功后接管 task 的所有权；失败时在返回前释放，调用方无需处理。
 * 2026 BY white
 */
[[nodiscard]] MonitorThreadStatus StartMonitorThread(JNIEnv* env, jobject preferred_loader,
                                                     const MonitorThreadConfig& config,
                                                     std::unique_ptr<MonitorTask> task) noexcept;

}  // namespace fod::monitor
