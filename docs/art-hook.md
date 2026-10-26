# ART（Java 方法）hook

[← 返回首页](../README.md) · [插件编写](plugins.md) · [Hook 写法](hooking.md) · [ART hook](art-hook.md) · [内置插件](builtin-plugins.md) · [构建与部署](build.md) · [日志与排查](troubleshooting.md) · [架构](architecture.md)

三个声明式宏只作用于原生指令。运行在 ART 上的 Java 方法可能解释执行，也可能已被 JIT/AOT 编译，
改写入口地址既不稳定也覆盖不全，这类目标由底层基于 LSPlant 的 `easy::art::ArtHook` 接管：

```cpp
#include "easy_hook/art/art_hook.hpp"

// 需要一个具备隐藏 API 访问权限的 JNIEnv，初始化一次即可
const auto status = easy::art::ArtHook::Init(env);

// backup 为可反射调用的原方法
jobject backup = easy::art::ArtHook::HookMethod(env, target_method, hooker_object,
                                                callback_method);
```

完整可运行示例见 [`plugins/art_demo/`](../plugins/art_demo)，它演示了这类 hook 绕不开的三件事：

- **回调类必须随 so 分发**：LSPlant 需要一个带 `callback(Object[])` 的 Java 对象。示例把回调类编译成 dex 内嵌进 so，
  运行时用 `InMemoryDexClassLoader` 从内存加载。改动回调逻辑后需运行 `tools/build_art_demo_dex.sh` 重新生成。
- **进程判定不能在 fork 后立刻做**：刚 fork 出的应用进程尚未改名，`/proc/self/cmdline` 仍是 zygote。
  示例把判定推迟到安装线程，等进程名稳定后再决定是否安装。
- **要挂对取值路径**：XML 里的 `android:title` 由 `TypedArray` 直接读字符串池，不经过 `Resources.getText`；
  `TextView.setText` 返回 void，只看返回值拦不到。示例因此分三层覆盖，并支持在调用原方法前改写入参。

**与 LSPosed 共存**：LSPlant 静态并入 `libeasyhook.so`，配合 `-Wl,--exclude-libs,ALL` 不导出符号；
生成桩类名从 `LSPHooker_` 改为 `EasyHooker_`，避免撞名。
