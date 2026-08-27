#include "easy_hook/art/art_hook.hpp"

#include "easy_hook/art/elf_symbol_table.hpp"
#include "easy_hook/core/log.hpp"
#include "easy_hook/dynamic_hook.hpp"

#include <lsplant.hpp>

#include <atomic>
#include <mutex>
#include <string_view>

/**
 * 2026年08月28日00时58分
 * ART hook 门面的实现：把 LSPlant 的初始化信息用框架组件填齐并转发方法级接口。
 * 2026 BY white
 */

namespace easy::art {
namespace {

constexpr std::string_view kLogTag = "art-hook";

/**
 * 2026年08月28日00时58分
 * libart.so 的符号表。必须具备静态存储期：LSPlant 保存的符号解析回调直接引用它，
 * 其生命周期要覆盖整个进程，任何在初始化之后发生的符号查询都依赖它仍然有效。
 * 2026 BY white
 */
ElfSymbolTable g_libart_symbols;

/**
 * 2026年08月28日00时58分
 * 初始化串行化锁与状态位。Init 可能被多个进程初始化路径触发，必须保证只成功执行一次。
 * 2026 BY white
 */
std::mutex g_init_mutex;
std::atomic<bool> g_initialized{false};

/**
 * 2026年08月28日00时58分
 * 与 LSPosed 共存所需的独立生成命名。
 * LSPlant 默认用 "LSPHooker_" 作为桩类名，LSPosed 也沿用该默认值；
 * 若两者在同一进程内以相同类名生成桩类，会相互覆盖或冲突。
 * 这里全部改用 easy 专属前缀，确保本框架生成的桩类与 LSPosed 的互不干扰。
 * 2026 BY white
 */
constexpr std::string_view kGeneratedClassName = "EasyHooker_";
constexpr std::string_view kGeneratedSourceName = "EASY";
constexpr std::string_view kGeneratedFieldName = "easyHooker";
constexpr std::string_view kGeneratedMethodName = "{target}";

}  // namespace

const char* ArtHookInitStatusName(ArtHookInitStatus status) noexcept {
    switch (status) {
        case ArtHookInitStatus::kOk:
            return "ok";
        case ArtHookInitStatus::kAlreadyInitialized:
            return "already_initialized";
        case ArtHookInitStatus::kInvalidEnvironment:
            return "invalid_environment";
        case ArtHookInitStatus::kLibartSymbolsUnavailable:
            return "libart_symbols_unavailable";
        case ArtHookInitStatus::kLsplantInitFailed:
            return "lsplant_init_failed";
    }
    return "unknown";
}

ArtHookInitStatus ArtHook::Init(JNIEnv* env) noexcept {
    if (env == nullptr) {
        return ArtHookInitStatus::kInvalidEnvironment;
    }

    const std::lock_guard<std::mutex> lock(g_init_mutex);
    if (g_initialized.load(std::memory_order_acquire)) {
        return ArtHookInitStatus::kAlreadyInitialized;
    }

    if (!g_libart_symbols.loaded()) {
        if (!g_libart_symbols.Load("libart.so")) {
            core::Log::Print(core::LogLevel::kError, kLogTag,
                             "failed to load libart.so symbol table");
            return ArtHookInitStatus::kLibartSymbolsUnavailable;
        }
        core::Log::Print(core::LogLevel::kInfo, kLogTag,
                         "libart.so symbols loaded: dynsym=%zu debug=%zu total=%zu",
                         g_libart_symbols.dynamic_symbol_count(),
                         g_libart_symbols.debug_symbol_count(),
                         g_libart_symbols.total_symbol_count());
    }

    lsplant::InitInfo info{};
    info.inline_hooker = [](void* target, void* hooker) -> void* {
        void* backup = nullptr;
        if (easy::HookFunction(target, hooker, &backup)) {
            return backup;
        }
        return nullptr;
    };
    info.inline_unhooker = [](void* func) -> bool { return easy::UnhookFunction(func); };
    info.art_symbol_resolver = [](std::string_view name) -> void* {
        return g_libart_symbols.Find(name);
    };
    info.art_symbol_prefix_resolver = [](std::string_view prefix) -> void* {
        return g_libart_symbols.FindByPrefix(prefix);
    };
    info.generated_class_name = kGeneratedClassName;
    info.generated_source_name = kGeneratedSourceName;
    info.generated_field_name = kGeneratedFieldName;
    info.generated_method_name = kGeneratedMethodName;

    if (!lsplant::Init(env, info)) {
        core::Log::Print(core::LogLevel::kError, kLogTag, "lsplant::Init failed");
        return ArtHookInitStatus::kLsplantInitFailed;
    }

    g_initialized.store(true, std::memory_order_release);
    core::Log::Print(core::LogLevel::kInfo, kLogTag, "ART hook engine initialized");
    return ArtHookInitStatus::kOk;
}

bool ArtHook::initialized() noexcept { return g_initialized.load(std::memory_order_acquire); }

jobject ArtHook::HookMethod(JNIEnv* env, jobject target_method, jobject hooker_object,
                            jobject callback_method) noexcept {
    if (env == nullptr || target_method == nullptr || hooker_object == nullptr ||
        callback_method == nullptr) {
        return nullptr;
    }
    if (!g_initialized.load(std::memory_order_acquire)) {
        core::Log::Print(core::LogLevel::kError, kLogTag, "HookMethod before Init");
        return nullptr;
    }
    return lsplant::Hook(env, target_method, hooker_object, callback_method);
}

bool ArtHook::UnhookMethod(JNIEnv* env, jobject target_method) noexcept {
    if (env == nullptr || target_method == nullptr ||
        !g_initialized.load(std::memory_order_acquire)) {
        return false;
    }
    return lsplant::UnHook(env, target_method);
}

bool ArtHook::IsMethodHooked(JNIEnv* env, jobject method) noexcept {
    if (env == nullptr || method == nullptr || !g_initialized.load(std::memory_order_acquire)) {
        return false;
    }
    return lsplant::IsHooked(env, method);
}

bool ArtHook::Deoptimize(JNIEnv* env, jobject method) noexcept {
    if (env == nullptr || method == nullptr || !g_initialized.load(std::memory_order_acquire)) {
        return false;
    }
    return lsplant::Deoptimize(env, method);
}

}  // namespace easy::art
