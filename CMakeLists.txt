cmake_minimum_required(VERSION 3.28)

project(easy_hook LANGUAGES C CXX)

option(EASY_HOOK_DEBUG_DEFAULT "Enable framework logging by default" ON)

#[[
2026年08月27日21时54分
框架自身代码使用严格告警，用户 hook 代码使用宽松告警。
两者分成不同的目标编译，避免新手在写替换函数时被 -Wconversion 之类的告警劝退，
同时保证框架本体的质量门槛不被降低。
2026 BY white
]]
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
    src/pattern/byte_pattern.cpp
    src/locator/symbol_locator.cpp
    src/locator/offset_locator.cpp
    src/locator/pattern_locator.cpp
    src/backend/dobby_backend.cpp
    src/loader/module_load_watcher.cpp
    src/runtime/bootstrap.cpp
)

#[[
2026年08月27日21时54分
自动收录 user 目录下的全部 cpp，使用者新增 hook 文件后无需改动本文件。
CONFIGURE_DEPENDS 让构建系统在文件增删时自动重新配置。
2026 BY white
]]
file(GLOB_RECURSE EASY_HOOK_USER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/user/*.cpp")
if(NOT EASY_HOOK_USER_SOURCES)
    message(FATAL_ERROR "No hook source found under user/. Add at least one .cpp there.")
endif()
list(LENGTH EASY_HOOK_USER_SOURCES EASY_HOOK_USER_SOURCE_COUNT)
message(STATUS "[Easy Hook] collected ${EASY_HOOK_USER_SOURCE_COUNT} user hook source(s)")

if(ANDROID)
    include(cmake/AndroidToolchainChecks.cmake)
    easy_hook_validate_android_toolchain()

    #[[
    2026年08月27日21时54分
    Dobby 仅提供 inline hook 与符号解析两项能力。
    导入表替换插件只在 Darwin 下编译且实现基于 Mach-O，在 Android 上不可用，因此不启用。
    符号解析插件必须保持开启，框架依赖它解析目标模块中未导出的内部符号。
    2026 BY white
    ]]
    set(DOBBY_DEBUG OFF CACHE BOOL "" FORCE)
    set(Plugin.SymbolResolver ON CACHE BOOL "" FORCE)
    set(Plugin.ImportTableReplace OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(BUILD_TEST OFF CACHE BOOL "" FORCE)
    add_subdirectory(dobby EXCLUDE_FROM_ALL)

    #[[
    2026年08月27日22时04分
    把 Dobby 的头文件目录标记为系统头。dobby.h 使用了命名可变参数宏等 GNU 扩展，
    在框架自身的严格告警下会持续刷屏，掩盖我们真正需要关注的告警。
    2026 BY white
    ]]
    get_target_property(EASY_HOOK_DOBBY_INCLUDES dobby_static INTERFACE_INCLUDE_DIRECTORIES)
    if(EASY_HOOK_DOBBY_INCLUDES)
        set_target_properties(dobby_static PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${EASY_HOOK_DOBBY_INCLUDES}")
    endif()

    #[[
    2026年08月27日21时54分
    用户 hook 编译为 OBJECT 库而不是静态库。
    静态库中未被引用的目标文件会被链接器丢弃，那样每个 hook 的自注册静态对象都不会执行，
    所有 hook 都将静默失效。OBJECT 库的目标文件会被无条件链入，能保证注册一定发生。
    2026 BY white
    ]]
    #[[
    2026年08月28日00时02分
    user 目录本身也加入头文件搜索路径，使用者的多文件功能可以按 "模块/头文件.hpp"
    的形式相互引用，而不必写相对路径。
    2026 BY white
    ]]
    add_library(easy_hook_user_objects OBJECT ${EASY_HOOK_USER_SOURCES})
    target_include_directories(easy_hook_user_objects PRIVATE include user)
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
            log
            dl
    )
endif()
