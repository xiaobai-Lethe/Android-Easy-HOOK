cmake_minimum_required(VERSION 3.28)

project(easy_hook LANGUAGES C CXX)

option(EASY_HOOK_DEBUG_DEFAULT "Enable framework logging by default" ON)

add_library(easy_hook_strict_options INTERFACE)
target_compile_features(easy_hook_strict_options INTERFACE cxx_std_23)
target_compile_options(easy_hook_strict_options INTERFACE
    $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall;-Wextra;-Wpedantic;-Wconversion;-Wsign-conversion>
)
target_compile_definitions(easy_hook_strict_options INTERFACE
    EASY_HOOK_DEBUG_DEFAULT=$<BOOL:${EASY_HOOK_DEBUG_DEFAULT}>
)

add_library(easy_hook_user_options INTERFACE)
target_compile_features(easy_hook_user_options INTERFACE cxx_std_23)
target_compile_options(easy_hook_user_options INTERFACE
    $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall;-Wextra>
)

set(EASY_HOOK_FRAMEWORK_SOURCES
    src/core/log.cpp
    src/core/target.cpp
    src/core/install_state.cpp
    src/core/registration.cpp
    src/core/process_filter.cpp
    src/core/registry.cpp
    src/core/engine.cpp
    src/core/runtime.cpp
    src/core/dynamic_hook.cpp
    src/module/module_lookup.cpp
    src/art/xz_decompressor.cpp
    src/art/elf_symbol_table.cpp
    src/art/art_hook.cpp
    src/pattern/byte_pattern.cpp
    src/locator/symbol_locator.cpp
    src/locator/offset_locator.cpp
    src/locator/pattern_locator.cpp
    src/backend/dobby_backend.cpp
    src/loader/module_load_watcher.cpp
    src/runtime/bootstrap.cpp
)

file(GLOB_RECURSE EASY_HOOK_PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/plugins/*.cpp")
file(GLOB_RECURSE EASY_HOOK_OWN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/user/*.cpp")
set(EASY_HOOK_USER_SOURCES ${EASY_HOOK_PLUGIN_SOURCES} ${EASY_HOOK_OWN_SOURCES})
if(NOT EASY_HOOK_USER_SOURCES)
    message(FATAL_ERROR
        "No hook source found. Add at least one .cpp under user/ or plugins/.")
endif()
list(LENGTH EASY_HOOK_PLUGIN_SOURCES EASY_HOOK_PLUGIN_SOURCE_COUNT)
list(LENGTH EASY_HOOK_OWN_SOURCES EASY_HOOK_OWN_SOURCE_COUNT)
message(STATUS "[Easy Hook] plugins/ ${EASY_HOOK_PLUGIN_SOURCE_COUNT} source(s), "
               "user/ ${EASY_HOOK_OWN_SOURCE_COUNT} source(s)")

if(ANDROID)
    include(cmake/AndroidToolchainChecks.cmake)
    easy_hook_validate_android_toolchain()

    #[[
    2026年08月28日01时52分
    ImportTableReplace 不可开启：该插件只在 Darwin 下编译且实现基于 Mach-O，在 Android 上是死代码。
    SymbolResolver 不可关闭：框架依赖它解析目标模块中未导出的内部符号。
    2026 BY white
    ]]
    set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
    set(Plugin.SymbolResolver ON CACHE BOOL "" FORCE)
    set(Plugin.ImportTableReplace OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(dobby EXCLUDE_FROM_ALL)

    #[[
    2026年08月28日01时52分
    第三方头标记为系统头，否则它们的 GNU 扩展用法会在框架的严格告警下持续刷屏，
    淹没框架自身的告警。lsplant_static 下方同理。
    2026 BY white
    ]]
    get_target_property(EASY_HOOK_DOBBY_INCLUDES dobby_static INTERFACE_INCLUDE_DIRECTORIES)
    if(EASY_HOOK_DOBBY_INCLUDES)
        set_target_properties(dobby_static PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${EASY_HOOK_DOBBY_INCLUDES}")
    endif()

    #[[
    2026年08月28日01时52分
    只编译 LSPlant 与 DexBuilder 的静态变体：它们的 shared 变体带 -flto，
    开启后会把整条链路拖入 LTO；静态并入还能省去额外的 so 依赖。
    两者使用 C++20 命名模块，因此构建必须用 Ninja 生成器，Makefile 无法处理模块依赖。
    2026 BY white
    ]]
    set(LSPLANT_BUILD_SHARED OFF CACHE INTERNAL "" FORCE)
    set(LSPLANT_STANDALONE OFF CACHE INTERNAL "" FORCE)
    set(DEBUG_SYMBOLS_PATH "${CMAKE_BINARY_DIR}/lsplant-symbols" CACHE INTERNAL "" FORCE)
    add_subdirectory(
        third_party/lsplant/lsplant/src/main/jni
        "${CMAKE_BINARY_DIR}/lsplant"
        EXCLUDE_FROM_ALL)

    get_target_property(EASY_HOOK_LSPLANT_INCLUDES lsplant_static INTERFACE_INCLUDE_DIRECTORIES)
    if(EASY_HOOK_LSPLANT_INCLUDES)
        set_target_properties(lsplant_static PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${EASY_HOOK_LSPLANT_INCLUDES}")
    endif()

    #[[
    2026年08月27日21时54分
    用户 hook 编译为 OBJECT 库而不是静态库。
    静态库中未被引用的目标文件会被链接器丢弃，那样每个 hook 的自注册静态对象都不会执行，
    所有 hook 都将静默失效。OBJECT 库的目标文件会被无条件链入，能保证注册一定发生。
    2026 BY white
    ]]
    add_library(easy_hook_user_objects OBJECT ${EASY_HOOK_USER_SOURCES})
    target_include_directories(easy_hook_user_objects PRIVATE include plugins user)
    target_link_libraries(easy_hook_user_objects PRIVATE easy_hook_user_options)

    add_library(easyhook SHARED ${EASY_HOOK_FRAMEWORK_SOURCES})
    set_target_properties(easyhook PROPERTIES
        OUTPUT_NAME easyhook
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    target_include_directories(easyhook PUBLIC include)
    target_link_options(easyhook PRIVATE -Wl,--exclude-libs,ALL)
    target_link_libraries(easyhook
        PRIVATE
            easy_hook_strict_options
            easy_hook_user_objects
            dobby_static
            lsplant_static
            log
            dl
    )

    set_target_properties(easyhook easy_hook_user_objects PROPERTIES
        CXX_SCAN_FOR_MODULES OFF)

    #[[
    2026年08月28日01时52分
    独立探针：不注入进程、不改动系统文件，用 adb shell 直接跑，
    可零风险验证从被裁剪的 libart.so 中解析符号这一最易出问题的环节。
    2026 BY white
    ]]
    add_executable(easyhook_symprobe
        tools/symprobe/main.cpp
        src/core/log.cpp
        src/module/module_lookup.cpp
        src/art/xz_decompressor.cpp
        src/art/elf_symbol_table.cpp
    )
    target_include_directories(easyhook_symprobe PRIVATE include)
    target_link_libraries(easyhook_symprobe PRIVATE easy_hook_strict_options log dl)
    set_target_properties(easyhook_symprobe PROPERTIES CXX_SCAN_FOR_MODULES OFF)
endif()
