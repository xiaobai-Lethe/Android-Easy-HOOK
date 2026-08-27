#include "fod/monitor/monitor_thread.hpp"

#include "fod/core/debug.hpp"

#include <cerrno>
#include <new>
#include <string>
#include <utility>

#if defined(__ANDROID__)
#include <pthread.h>
#include <time.h>
#endif

namespace fod::monitor {
namespace {

#if defined(__ANDROID__)

/**
 * 2026年08月27日23时10分
 * 监控线程运行所需的全部上下文。由启动方构造，运行结束后在线程内销毁。
 * 2026 BY white
 */
struct MonitorContext final {
    JavaVM* vm{};
    jobject preferred_loader{};
    MonitorThreadConfig config;
    std::unique_ptr<MonitorTask> task;
    std::string log_tag;
};

/**
 * 2026年08月27日23时10分
 * 按毫秒休眠，被信号打断时继续把剩余时间睡完，避免监控节奏被随机缩短。
 * 2026 BY white
 */
void SleepForMilliseconds(int milliseconds) noexcept {
    if (milliseconds <= 0) {
        return;
    }
    timespec remaining{
        .tv_sec = milliseconds / 1000,
        .tv_nsec = static_cast<long>(milliseconds % 1000) * 1000000L,
    };
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {
    }
}

void* RunMonitor(void* opaque_context) noexcept {
    std::unique_ptr<MonitorContext> context(static_cast<MonitorContext*>(opaque_context));
    const std::string_view tag(context->log_tag);

    jni::ScopedThreadEnv scoped_env(context->vm);
    JNIEnv* env = scoped_env.get();
    if (env == nullptr) {
        core::Debug::Log(core::LogLevel::kError, tag, "failed to attach monitor thread to ART");
        return nullptr;
    }

    const int max_attempts = context->config.max_attempts;
    core::Debug::Log(core::LogLevel::kInfo, tag, "monitor started attempts=%d interval=%dms",
                     max_attempts, context->config.interval_ms);

    for (int attempt = 0; max_attempts <= 0 || attempt < max_attempts; ++attempt) {
        if (!context->task->Tick(env, context->preferred_loader, attempt)) {
            break;
        }
        const bool has_next = max_attempts <= 0 || attempt + 1 < max_attempts;
        if (has_next) {
            SleepForMilliseconds(context->config.interval_ms);
        }
    }

    context->task->Finish(env, context->preferred_loader);

    if (context->preferred_loader != nullptr) {
        env->DeleteGlobalRef(context->preferred_loader);
        context->preferred_loader = nullptr;
    }
    core::Debug::Log(core::LogLevel::kInfo, tag, "monitor finished");
    return nullptr;
}

#endif

}  // namespace

const char* MonitorThreadStatusName(MonitorThreadStatus status) noexcept {
    switch (status) {
        case MonitorThreadStatus::kStarted:
            return "started";
        case MonitorThreadStatus::kAlreadyStarted:
            return "already_started";
        case MonitorThreadStatus::kInvalidConfig:
            return "invalid_config";
        case MonitorThreadStatus::kMissingJniEnv:
            return "missing_jni_env";
        case MonitorThreadStatus::kMissingJavaVm:
            return "missing_java_vm";
        case MonitorThreadStatus::kLoaderReferenceFailed:
            return "loader_reference_failed";
        case MonitorThreadStatus::kAllocationFailed:
            return "allocation_failed";
        case MonitorThreadStatus::kThreadStartFailed:
            return "thread_start_failed";
        case MonitorThreadStatus::kUnsupportedPlatform:
            return "unsupported_platform";
    }
    return "invalid";
}

MonitorThreadStatus StartMonitorThread(JNIEnv* env, jobject preferred_loader,
                                       const MonitorThreadConfig& config,
                                       std::unique_ptr<MonitorTask> task) noexcept {
    if (task == nullptr || config.interval_ms <= 0 || config.max_attempts < 0) {
        return MonitorThreadStatus::kInvalidConfig;
    }
    if (env == nullptr) {
        return MonitorThreadStatus::kMissingJniEnv;
    }

#if defined(__ANDROID__)
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK || vm == nullptr) {
        return MonitorThreadStatus::kMissingJavaVm;
    }

    /**
     * 2026年08月27日23时10分
     * 类加载器要跨线程使用，必须升级为全局引用；局部引用出了当前 JNI 调用即失效。
     * 2026 BY white
     */
    jobject loader_reference = nullptr;
    if (preferred_loader != nullptr) {
        loader_reference = env->NewGlobalRef(preferred_loader);
        if (env->ExceptionCheck() == JNI_TRUE) {
            env->ExceptionClear();
        }
        if (loader_reference == nullptr) {
            return MonitorThreadStatus::kLoaderReferenceFailed;
        }
    }

    auto release_loader = [env, &loader_reference]() noexcept {
        if (loader_reference != nullptr) {
            env->DeleteGlobalRef(loader_reference);
            loader_reference = nullptr;
        }
    };

    auto* context = new (std::nothrow) MonitorContext{
        .vm = vm,
        .preferred_loader = loader_reference,
        .config = config,
        .task = std::move(task),
        .log_tag = std::string(config.log_tag),
    };
    if (context == nullptr) {
        release_loader();
        return MonitorThreadStatus::kAllocationFailed;
    }

    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0) {
        delete context;
        release_loader();
        return MonitorThreadStatus::kThreadStartFailed;
    }

    /**
     * 2026年08月27日23时10分
     * 监控线程与宿主进程同生命周期，没有任何一处会去 join 它，
     * 因此必须创建为分离状态，否则线程退出后其资源不会被回收。
     * 2026 BY white
     */
    if (pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED) != 0) {
        pthread_attr_destroy(&attributes);
        delete context;
        release_loader();
        return MonitorThreadStatus::kThreadStartFailed;
    }

    pthread_t thread{};
    const int create_result = pthread_create(&thread, &attributes, RunMonitor, context);
    pthread_attr_destroy(&attributes);
    if (create_result != 0) {
        delete context;
        release_loader();
        return MonitorThreadStatus::kThreadStartFailed;
    }

    return MonitorThreadStatus::kStarted;
#else
    static_cast<void>(preferred_loader);
    return MonitorThreadStatus::kUnsupportedPlatform;
#endif
}

}  // namespace fod::monitor
