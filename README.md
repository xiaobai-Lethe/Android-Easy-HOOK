# Easy Hook

**面向新手的 Android 原生 Hook 框架** —— 只写一个 cpp，声明要 hook 什么、hook 成什么，其余全部自动完成。

![Android 11+](https://img.shields.io/badge/Android-11%2B-3DDC84?logo=android&logoColor=white)
![ABI](https://img.shields.io/badge/ABI-arm64--v8a-blue)
![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-3.28%2B-064F8C?logo=cmake&logoColor=white)

## 这个框架解决什么问题

写一个 native hook，通常要自己处理符号解析、模块基址换算、hook 安装、原函数保存、
目标库还没加载时的等待，以及各种失败情况的排查。这些和你真正想做的事无关。

Easy Hook 把这些全部收进引擎，你只需要在 `user/` 目录写一个 cpp，声明两件事：

- **hook 什么**：模块名 + 符号名、模块名 + 偏移，或者一段字节特征码
- **hook 成什么**：一个函数体，需要时用 `EASY_ORIGINAL(...)` 调回原逻辑

不需要写注册代码、初始化代码，不需要改 CMakeLists，也不需要关心目标库有没有加载。

## 功能支持情况

| 能力 | 状态 | 说明 |
| --- | :---: | --- |
| 按符号名 hook 原生函数 | ✅ | 支持导出符号与 `.symtab` 中的内部符号 |
| 按模块 + 偏移 hook | ✅ | 自动换算加载基址，可直接填 IDA 里读到的偏移 |
| 按字节特征码 hook | ✅ | 支持通配符，命中多处会拒绝安装 |
| 目标库未加载时自动补装 | ✅ | 监听模块加载，加载后立即补装 |
| 运行期动态 hook 任意地址 | ✅ | `easy::HookFunction`，用于地址只能在运行期取得的目标 |
| 按进程过滤 | ✅ | `EASY_HOOK_ONLY_IN`，支持 `包名:子进程` 前缀匹配 |
| ART（Java 方法）hook | ✅ | 接入 LSPlant，见 [ART hook](docs/art-hook.md) |
| 从 `.gnu_debugdata` 解析 libart 符号 | ✅ | 内置 XZ 解压，无需额外依赖 |
| 与 LSPosed 共存 | ✅ | LSPlant 静态内嵌且不导出符号，桩类名独立 |
| 卸载已安装的 hook | ✅ | 原生用 `easy::UnhookFunction`，ART 用 `ArtHook::UnhookMethod` |
| JNI 原生方法 hook | 🚧 | 替换 `JNINativeMethod.fnPtr`，尚未完成 |
| 导入表 / PLT 替换 | ❌ | 暂未实现 |
| arm64-v8a 以外的 ABI | ❌ | 未测试 |

## 快速开始

**1. 写一个 hook。** 在仓库根目录新建 `user/`（git 不跟踪空目录，首次需要自己建），放入任意名字的 cpp：

```cpp
#include "easy_hook/easy_hook.hpp"

EASY_HOOK_SYMBOL(MyAtoi, "libc.so", "atoi", int, const char* text) {
    EASY_LOG("atoi(%s)", text != nullptr ? text : "<null>");
    return EASY_ORIGINAL(text);
}
```

宏的参数顺序固定为 `(名字, 目标定位..., 返回类型, 参数列表...)`。
第一个参数只用于日志和状态查询，随便起，不重复即可。

**2. 构建。**

```powershell
.\scripts\build-android.ps1
```

**3. 加载。** 产物是 `build-android-ninja/libeasyhook.so`，把它加载进目标进程即可生效。
ROM 预置、Zygisk 模块、手动 `dlopen` 都可以，框架不绑定任何一种加载方式。

更多写法见 [Hook 写法](docs/hooking.md)，构建细节见 [构建与部署](docs/build.md)。

## 文档

| 文档 | 内容 |
| --- | --- |
| [插件编写与配置](docs/plugins.md) | `user/` 与 `plugins/` 的区别、组织代码、开关内置插件 |
| [Hook 写法](docs/hooking.md) | 符号 / 偏移 / 特征码三种定位、进程过滤、状态查询、安装时机 |
| [ART（Java 方法）hook](docs/art-hook.md) | 基于 LSPlant 的 Java 方法 hook 与注意事项 |
| [内置插件](docs/builtin-plugins.md) | Flyme 屏下指纹 / AOD / 震动修复等现成插件 |
| [构建与部署](docs/build.md) | 环境要求、构建命令、预置进 ROM 的步骤 |
| [日志与排查](docs/troubleshooting.md) | 日志开关、安装状态含义、排查顺序 |
| [架构](docs/architecture.md) | 目录结构与设计要点 |

## 开源依赖

原生指令改写交给 Dobby，ART 方法改写交给 LSPlant；本框架负责把它们统一到一套声明式接口之下，
并补齐符号解析、延迟安装、进程过滤与排查手段。

| 项目 | 用途 | 许可证 |
| --- | --- | --- |
| [Dobby](https://github.com/jmpews/Dobby) | 原生函数 inline hook 与符号解析 | Apache-2.0 |
| [LSPlant](https://github.com/LSPosed/LSPlant) | ART（Java 方法）hook 引擎 | LGPL-3.0 |
| [DexBuilder](https://github.com/LSPosed/DexBuilder) | LSPlant 生成桩类所需 | LGPL-3.0 |
| [parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap) | DexBuilder 的哈希容器依赖 | Apache-2.0 |
