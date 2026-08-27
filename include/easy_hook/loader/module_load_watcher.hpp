#pragma once

namespace easy::loader {

/**
 * 2026年08月27日21时46分
 * 模块加载监听器。通过 hook android_dlopen_ext 感知新模块加载，在其返回后触发注册表
 * 对挂起项补装，使用户无需关心目标 so 是否已经就绪。
 * 只安装一次，且只在成功取得原函数地址后才生效。
 * 2026 BY white
 */
class ModuleLoadWatcher final {
public:
    ModuleLoadWatcher() = delete;

    /**
     * 2026年08月27日21时46分
     * 安装监听。已安装或符号不可用时返回 false，此时框架退化为仅在启动时安装一次。
     * 2026 BY white
     */
    [[nodiscard]] static bool Install() noexcept;

    [[nodiscard]] static bool installed() noexcept;
};

}  // namespace easy::loader
