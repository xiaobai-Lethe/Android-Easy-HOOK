#include "easy_hook/core/log.hpp"
#include "easy_hook/core/runtime.hpp"

#include <string_view>

namespace easy::runtime {
namespace {

constexpr std::string_view kLogTag = "bootstrap";

}  // namespace

/**
 * 2026年08月27日21时48分
 * 库入口。libeasyhook.so 被加载时由动态链接器调用，完成框架启动。
 * 放在构造函数中是为了让使用者无需在宿主侧编写任何初始化代码，
 * 无论通过 ROM 预置、Zygisk 还是手动 dlopen 加载都能自动生效。
 * 2026 BY white
 */
__attribute__((constructor)) void EasyHookInitialize() {
    core::Log::InitializeFromProperties();
    core::Log::Print(core::LogLevel::kInfo, kLogTag, "libeasyhook loaded");
    const core::RuntimeStartStatus status = core::Runtime::Start();
    core::Log::Print(core::LogLevel::kInfo, kLogTag, "runtime start status=%s",
                     core::Runtime::StartStatusName(status));
}

}  // namespace easy::runtime
