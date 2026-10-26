# 插件编写与配置

[← 返回首页](../README.md) · [插件编写](plugins.md) · [Hook 写法](hooking.md) · [ART hook](art-hook.md) · [内置插件](builtin-plugins.md) · [构建与部署](build.md) · [日志与排查](troubleshooting.md) · [架构](architecture.md)

hook 源码分两处存放，都会被自动收录并加入头文件搜索路径：

| 目录 | 默认 | 用途 |
| --- | --- | --- |
| `user/` | 全部编译 | 你自己的插件，新建 `.cpp` 即生效 |
| `plugins/` | 全部不编译 | 随框架分发的成品 / 参考插件，按需开启 |

## 组织自己的代码

功能变复杂后可以自行分目录。`user/` 在头文件搜索路径中，可以直接按 `模块/头文件.hpp` 引用：

```text
user/
└── my_plugin/
    ├── my_plugin.cpp
    └── core/
        ├── worker.hpp
        └── worker.cpp
```

```cpp
#include "my_plugin/core/worker.hpp"
```

## 开关内置插件

`plugins/` 下每个子目录是一个可选插件。用 menuconfig 风格的交互脚本勾选：

```powershell
pwsh tools/plugin_config.ps1     # Windows：↑/↓ 移动、空格勾选、回车保存
```

```bash
tools/plugin_config.sh           # Linux / macOS
```

结果写入根目录的 `plugins.config`（每行一个文件夹名，`#` 开头为注释），CMake 配置时自动读取。
CI 中也可以直接覆盖：

```bash
cmake -DEASY_HOOK_PLUGINS="flyme-common;flyme-fix-aod" ...
```

已启用插件之间按 `插件文件夹名/模块/头文件.hpp` 互相引用。
构建要求 `user/` 与已启用插件合计至少有一个 `.cpp`。
