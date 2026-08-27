/**
 * 2026年08月27日22时44分
 * 示例与模板文件。本文件默认不安装任何 hook，全部示例都以 EASY_HOOK_EXAMPLE_ENABLED
 * 开关关闭，仅作为三种定位方式的写法参考。要实际使用时，把需要的示例复制到自己的
 * cpp 文件中修改，或把下面的开关打开。
 * 2026 BY white
 */

#include "easy_hook/easy_hook.hpp"

/**
 * 2026年08月27日22时44分
 * 示例总开关。默认关闭。
 * 本框架通过 app_process64 注入后会存在于 zygote 及其派生的每一个进程中，
 * 因此默认不能安装任何会影响全系统的 hook，否则一个失误就会导致整机无法开机。
 * 2026 BY white
 */
#define EASY_HOOK_EXAMPLE_ENABLED 0

#if EASY_HOOK_EXAMPLE_ENABLED

#include <fcntl.h>

/**
 * 2026年08月27日22时44分
 * 按模块名加符号名定位的示例。符号可以是未导出的内部符号。
 *
 * 危险示例，仅作语法演示，切勿在注入 zygote 的场景下开启：
 * open 是被整个系统高频调用的基础函数，把它 hook 掉并在替换函数里打日志，
 * 会让每个进程的每一次文件打开都产生日志，严重拖慢系统；
 * 更关键的是，一旦替换函数内部又间接调用了 open，就会形成同线程重入。
 * 框架已在日志层加了重入保护并改用裸系统调用，不会再因此死锁，
 * 但选择 hook 目标时仍应避开这类被高频调用的基础函数。
 * 2026 BY white
 */
EASY_HOOK_SYMBOL(OpenFile, "libc.so", "open", int, const char* path, int flags) {
    EASY_LOG("open path=%s flags=0x%x", path != nullptr ? path : "<null>", flags);
    return EASY_ORIGINAL(path, flags);
}

/**
 * 2026年08月27日22时44分
 * 按模块名加相对虚拟地址定位的示例，偏移直接填 IDA 中显示的地址。
 * 目标模块未加载时该 hook 会自动挂起，等模块加载后由框架补装。
 * 使用时把模块名和偏移换成实际目标。
 * 2026 BY white
 */
EASY_HOOK_OFFSET(DamageCalculate, "libgame.so", 0x1A2B0, int, void* self, int raw_damage) {
    EASY_LOG("damage raw=%d forced to 0", raw_damage);
    return 0;
}

/**
 * 2026年08月27日22时44分
 * 按字节特征码定位的示例，?? 表示任意字节。
 * 特征码必须在目标模块可执行段中唯一命中，命中多处时框架会拒绝安装并提示加长特征码。
 * 使用时把模块名和特征码换成实际目标。
 * 2026 BY white
 */
EASY_HOOK_PATTERN(EncryptBuffer, "libgame.so", "FF 83 00 D1 ?? ?? 00 F9", int, void* buffer,
                  int length) {
    EASY_LOG("encrypt length=%d", length);
    return EASY_ORIGINAL(buffer, length);
}

/**
 * 2026年08月27日22时44分
 * 进程过滤示例。不声明时注入到哪个进程就在哪个进程生效；
 * 声明后只有匹配的进程会安装 hook，可重复声明以允许多个进程。
 * 注入 zygote 的场景下，只要 hook 不是面向全系统的，都应当加上这个限定。
 * 2026 BY white
 */
EASY_HOOK_ONLY_IN("com.example.game")

#endif  // EASY_HOOK_EXAMPLE_ENABLED
