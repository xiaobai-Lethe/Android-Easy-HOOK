# 架构

[← 返回首页](../README.md) · [插件编写](plugins.md) · [Hook 写法](hooking.md) · [ART hook](art-hook.md) · [内置插件](builtin-plugins.md) · [构建与部署](build.md) · [日志与排查](troubleshooting.md) · [架构](architecture.md)

```text
Android-Easy-HOOK/
├── include/easy_hook/
│   ├── easy_hook.hpp      使用者唯一需要包含的总头
│   ├── macros.hpp         三个 hook 宏与辅助宏
│   ├── core/              日志、目标规格、注册表、引擎、运行时、进程过滤
│   ├── module/            基于 dl_iterate_phdr 的模块查询
│   ├── pattern/           字节特征码解析与搜索
│   ├── locator/           符号、偏移、特征码三种定位器
│   ├── backend/           inline hook 抽象接口与 Dobby 实现
│   ├── art/               ART 方法 hook：libart 符号表、XZ 解压、LSPlant 门面
│   └── loader/            模块加载监听
├── src/                   与 include 一一对应的实现
├── plugins/               内置插件，默认不编译
├── user/                  你的插件，默认全部编译（需自行创建）
├── plugins.config         启用哪些内置插件
├── dobby/                 Dobby（原生 inline hook）
├── third_party/lsplant/   LSPlant（ART 方法 hook）
├── tools/                 插件选择器、回调 dex 生成脚本、符号解析探针
└── scripts/               构建脚本
```

**设计要点**

- **引擎不依赖具体 hook 库**：只面向 `backend::InlineHookBackend` 接口编程，Dobby 只是其中一个实现。
- **注册表使用函数内静态单例**：自注册对象分散在各翻译单元的静态构造期，延迟创建以避开静态初始化顺序问题。
- **用户代码编译为 OBJECT 库**：静态库中未被引用的目标文件会被链接器丢弃，导致自注册不执行；OBJECT 库保证无条件链入。
- **分级告警**：框架本体开启 `-Wpedantic -Wconversion`，用户代码只开 `-Wall -Wextra`。
