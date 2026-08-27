/**
 * 2026年08月28日02时56分
 * ART 符号解析探针。
 *
 * 独立的命令行程序，用 adb shell 直接运行，不注入任何进程、不安装任何 hook、
 * 不改动任何系统文件。目的是在完全无风险的前提下先验证最难的一环：
 * 能否从被裁剪的 libart.so 中解析出 ART hook 所需的全部符号
 * （其中若干符号只存在于 XZ 压缩的 .gnu_debugdata 节区里）。
 *
 * 只有这一步被证实可靠，才值得把这套解析搬进注入到 zygote 的框架里。
 * 2026 BY white
 */

#include "easy_hook/art/elf_symbol_table.hpp"
#include "easy_hook/art/xz_decompressor.hpp"
#include "easy_hook/core/log.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <dlfcn.h>

namespace {

/**
 * 2026年08月28日02时56分
 * 一组等价符号。ART 的同一功能在不同系统版本上修饰名不同，
 * LSPlant 会依次尝试，因此只要组内命中任意一个即视为该功能可用。
 * 2026 BY white
 */
struct SymbolGroup final {
    const char* purpose;
    bool required;
    std::vector<const char*> candidates;
};

const std::vector<SymbolGroup>& RequiredSymbols() {
    static const std::vector<SymbolGroup> groups = {
        {"ArtMethod 去内建标记", true,
         {"_ZN3art9ArtMethod15SetNotIntrinsicEv"}},
        {"方法可读名", false,
         {"_ZN3art9ArtMethod12PrettyMethodEPS0_b", "_ZN3art12PrettyMethodEPNS_9ArtMethodEb"}},
        {"取方法签名简写", false,
         {"_ZN3artL15GetMethodShortyEP7_JNIEnvP10_jmethodID",
          "_ZN3art15GetMethodShortyEP7_JNIEnvP10_jmethodID"}},
        {"运行时单例", true, {"_ZN3art7Runtime9instance_E"}},
        {"设置运行时可调试", false,
         {"_ZN3art7Runtime17SetJavaDebuggableEb",
          "_ZN3art7Runtime20SetRuntimeDebugStateENS0_17RuntimeDebugStateE"}},
        {"当前线程", true, {"_ZN3art6Thread14CurrentFromGdbEv"}},
        {"全局暂停 构造", true, {"_ZN3art16ScopedSuspendAllC2EPKcb"}},
        {"全局暂停 析构", true, {"_ZN3art16ScopedSuspendAllD2Ev"}},
        {"GC 临界区 构造", true,
         {"_ZN3art2gc23ScopedGCCriticalSectionC2EPNS_6ThreadENS0_7GcCauseENS0_13CollectorTypeE"}},
        {"GC 临界区 析构", true, {"_ZN3art2gc23ScopedGCCriticalSectionD2Ev"}},
        {"JIT 迁移废弃方法", false,
         {"_ZN3art3jit12JitCodeCache18MoveObsoleteMethodEPNS_9ArtMethodES3_"}},
        {"类状态变更", true,
         {"_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS_11ClassStatusEPNS_6ThreadE",
          "_ZN3art6mirror5Class9SetStatusENS_6HandleIS1_EENS1_6StatusEPNS_6ThreadE",
          "_ZN3art6mirror5Class9SetStatusENS1_6StatusEPNS_6ThreadE"}},
        {"静态跳板修正", true,
         {"_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6ThreadENS_6ObjPtrINS_6mirror5ClassEEE",
          "_ZN3art11ClassLinker22FixupStaticTrampolinesENS_6ObjPtrINS_6mirror5ClassEEE",
          "_ZN3art11ClassLinker22FixupStaticTrampolinesEPNS_6mirror5ClassE"}},
        {"原生方法注册", true,
         {"_ZN3art11ClassLinker14RegisterNativeEPNS_6ThreadEPNS_9ArtMethodEPKv",
          "_ZN3art9ArtMethod14RegisterNativeEPKvb", "_ZN3art9ArtMethod14RegisterNativeEPKv"}},
        {"原生方法注销", false,
         {"_ZN3art11ClassLinker16UnregisterNativeEPNS_6ThreadEPNS_9ArtMethodE",
          "_ZN3art9ArtMethod16UnregisterNativeEv"}},
        {"解释器入口桥", true,
         {"art_quick_to_interpreter_bridge", "artInterpreterToCompiledCodeBridge"}},
        {"改用解释器入口", false,
         {"_ZNK3art11ClassLinker27SetEntryPointsToInterpreterEPNS_9ArtMethodE"}},
        {"是否走解释器", false,
         {"_ZN3art11ClassLinker30ShouldUseInterpreterEntrypointEPNS_9ArtMethodEPKv"}},
        {"初始化方法代码", false,
         {"_ZN3art15instrumentation15Instrumentation21InitializeMethodsCodeEPNS_9ArtMethodEPKv",
          "_ZN3art15instrumentation15Instrumentation23ReinitializeMethodsCodeEPNS_9ArtMethodE",
          "_ZN3art15instrumentation15Instrumentation40UpdateMethodsCodeToInterpreterEntryPointEPNS_"
          "9ArtMethodE"}},
        {"可见初始化回调", false,
         {"_ZN3art11ClassLinker26VisiblyInitializedCallback22MarkVisiblyInitializedEPNS_6ThreadE"}},
        {"DexFile 置信任", false,
         {"_ZN3artL18DexFile_setTrustedEP7_JNIEnvP7_jclassP8_jobject"}},
        {"JNI id 编码", false,
         {"_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEjNS_16ReflectiveHandleIT_EE",
          "_ZN3art3jni12JniIdManager15EncodeGenericIdINS_9ArtMethodEEEmNS_16ReflectiveHandleIT_"
          "EE"}},
    };
    return groups;
}

}  // namespace

int main() {
    easy::core::Log::SetEnabled(true);
    easy::core::Log::SetVerbose(true);

    std::printf("\n========== ART 符号解析探针 ==========\n\n");

    easy::art::XzDecompressor& decompressor = easy::art::XzDecompressor::Instance();
    std::printf("[XZ 解码器] 可用=%s  状态=%s\n\n", decompressor.available() ? "是" : "否",
                decompressor.status());

    /**
     * 2026年08月28日03时08分
     * 探针是普通命令行程序，自身并不加载 ART，因此先尝试把 libart.so 拉进本进程，
     * 这样才能拿到真实的运行时基址并验证地址换算。
     * 拉不进来时退回按路径读文件，此时只核对符号存在性。
     * 2026 BY white
     */
    void* handle = dlopen("libart.so", RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        handle = dlopen("/apex/com.android.art/lib64/libart.so", RTLD_NOW | RTLD_LOCAL);
    }
    std::printf("[libart] dlopen %s\n\n", handle != nullptr ? "成功，可验证运行时地址"
                                                            : "失败，退回按文件解析");

    easy::art::ElfSymbolTable table;
    bool ready = table.Load("libart.so");
    if (!ready) {
        for (const char* candidate : {"/apex/com.android.art/lib64/libart.so",
                                      "/system/lib64/libart.so"}) {
            if (table.LoadFromPath(candidate)) {
                ready = true;
                break;
            }
        }
    }
    if (!ready) {
        std::printf("加载 libart.so 符号表失败\n");
        return 1;
    }

    std::printf("\n[符号表] 路径=%s\n", table.path().c_str());
    std::printf("[符号表] load_bias=%p\n", reinterpret_cast<void*>(table.load_bias()));
    std::printf("[符号表] 文件自带=%zu  gnu_debugdata=%zu  合计=%zu\n\n",
                table.dynamic_symbol_count(), table.debug_symbol_count(),
                table.total_symbol_count());

    std::printf("========== LSPlant 所需符号逐项核对 ==========\n\n");

    int satisfied = 0;
    int missing_required = 0;
    int missing_optional = 0;

    for (const auto& group : RequiredSymbols()) {
        void* address = nullptr;
        const char* hit = nullptr;
        for (const char* candidate : group.candidates) {
            address = table.Find(candidate);
            if (address != nullptr) {
                hit = candidate;
                break;
            }
        }

        if (address != nullptr) {
            ++satisfied;
            std::printf("  [命中] %-18s %p\n", group.purpose, address);
            std::printf("         %s\n", hit);
        } else if (group.required) {
            ++missing_required;
            std::printf("  [缺失] %-18s  必需，LSPlant 无法工作\n", group.purpose);
            for (const char* candidate : group.candidates) {
                std::printf("         尝试过 %s\n", candidate);
            }
        } else {
            ++missing_optional;
            std::printf("  [可选缺失] %-14s  LSPlant 有降级路径\n", group.purpose);
        }
    }

    std::printf("\n========== 前缀匹配自检 ==========\n\n");
    for (const char* prefix : {"_ZN3art11ClassLinker22FixupStaticTrampolines",
                               "_ZN3art9ArtMethod14RegisterNative",
                               "_ZN3art6mirror5Class9SetStatus"}) {
        void* address = table.FindByPrefix(prefix);
        std::printf("  %-52s -> %p\n", prefix, address);
    }

    std::printf("\n========== 结论 ==========\n\n");
    std::printf("  命中 %d 组，必需缺失 %d 组，可选缺失 %d 组\n", satisfied, missing_required,
                missing_optional);
    if (missing_required == 0) {
        std::printf("  全部必需符号可解析，符号层具备接入 LSPlant 的条件。\n\n");
        return 0;
    }
    std::printf("  存在必需符号无法解析，需要先补齐候选名单再谈接入。\n\n");
    return 2;
}
