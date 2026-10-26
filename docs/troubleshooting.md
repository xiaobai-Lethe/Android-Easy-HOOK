# 日志与排查

[← 返回首页](../README.md) · [插件编写](plugins.md) · [Hook 写法](hooking.md) · [ART hook](art-hook.md) · [内置插件](builtin-plugins.md) · [构建与部署](build.md) · [日志与排查](troubleshooting.md) · [架构](architecture.md)

```bash
adb logcat -s "EASY-HOOK:*"
```

logcat 标签统一为 `EASY-HOOK`，下表中的模块名出现在消息正文里，用 `logcat -s` 按模块名过滤是抓不到的。
开启调试日志后还会追加写入 `/data/local/tmp/easy_hook.log`。

| 系统属性 | 默认值 | 作用 |
| --- | --- | --- |
| `persist.easy_hook.debug` | 构建配置，默认 `1` | 输出 Info 级日志并写入日志文件 |
| `persist.easy_hook.verbose` | `0` | 额外输出 Debug 级日志，含地址解析细节 |

<details>
<summary>日志模块名</summary>

| 模块名 | 关注内容 |
| --- | --- |
| `bootstrap` | 库是否被成功加载、运行时启动结果 |
| `runtime` | 进程过滤判定、模块监听是否就绪、首轮安装汇总 |
| `registry` | 每个 hook 的目标、状态与解析出的绝对地址 |
| `engine` | 单个 hook 的定位与安装结果 |
| `offset-locator` | 偏移换算细节，可与 IDA 核对 |
| `pattern-locator` | 特征码解析与命中次数 |
| `module-watcher` | 新模块加载与挂起项补装 |
| `art-hook` | ART 引擎初始化、libart 符号表规模 |
| `elf-symbols` | libart 符号解析结果，含 `.gnu_debugdata` 中的符号数 |
| `xz` | `.gnu_debugdata` 的解压情况 |
| `user` | `EASY_LOG` 输出的内容 |

</details>

## 安装状态

| 状态 | 含义与处理 |
| --- | --- |
| `installed` | 已成功安装 |
| `module_not_loaded` | 目标模块尚未加载，等待自动补装，通常无需处理 |
| `symbol_not_found` | 模块已加载但符号不存在，检查拼写与是否被裁剪 |
| `offset_out_of_range` | 偏移不在已映射段内，检查是否填错模块或填了文件偏移 |
| `pattern_invalid` | 特征码语法错误，或首字节是通配符 |
| `pattern_not_found` | 可执行段中没有命中，检查目标版本是否变化 |
| `pattern_ambiguous` | 命中多处，需要加长特征码 |
| `backend_failed` | 地址已解析但 inline hook 安装失败，通常是目标指令无法重定位 |
| `skipped_process_filter` | 当前进程不在 `EASY_HOOK_ONLY_IN` 允许列表中 |

## 排查顺序

1. **完全没有日志**：确认构造函数日志 `libeasyhook loaded` 是否出现。
   - 构造函数只在 zygote 中执行一次，fork 出的应用进程看不到该行属正常
   - 应用进程无权写 `/data/local/tmp/easy_hook.log`，只能从 logcat 观察
2. **提示 `no_registration`**：没有任何 hook 声明，或声明被 `#if 0` 关掉了
3. **一直 pending**：检查模块名拼写，以及 `module-watcher` 是否成功建立监听
4. **地址对不上**：开启 verbose，用 `offset-locator` 打印的 `load_bias` 与绝对地址和 IDA 核对
