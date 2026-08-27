# 你的插件写在这里

这个目录初始是空的。在这里新建任意 `.cpp`，构建时会自动加载，不需要改动 `CMakeLists.txt`。

最小的插件只要一个文件：

```cpp
#include "easy_hook/easy_hook.hpp"

EASY_HOOK_SYMBOL(MyHook, "libc.so", "atoi", int, const char* text) {
    EASY_LOG("atoi(%s)", text != nullptr ? text : "<null>");
    return EASY_ORIGINAL(text);
}
```

功能变复杂后可以自行分目录，本目录与 `plugins/` 都在头文件搜索路径中，
因此可以按 `模块/头文件.hpp` 的形式相互引用，不必写相对路径：

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

`plugins/` 下是随框架分发的参考插件，可以照着写，也可以直接删掉不用。
完整的宏说明、三种定位方式、ART hook 的写法和排查手册都在仓库根目录的 `README.md`。
