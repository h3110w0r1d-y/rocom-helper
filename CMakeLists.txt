cmake_minimum_required(VERSION 3.27)

project(roco_helper_plugin VERSION 2.0 LANGUAGES CXX)

set(ROCO_APP_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

set(ROCO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${ROCO_GENERATED_DIR}")

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/app_version.h.in"
        "${ROCO_GENERATED_DIR}/app_version_generated.h"
        @ONLY
)

if(WIN32)
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico" ROCO_APP_ICON)
    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/app.rc.in"
            "${ROCO_GENERATED_DIR}/app.rc"
            @ONLY
    )
endif()

# 项目依赖：Qt 用于应用、界面和远端 HTTP/SSE 客户端。
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

# 启用 Qt 代码生成，并尽量减少导出的符号。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE APP_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/*"
)

# macOS 构建为 .app bundle；Windows 构建为 GUI 子系统程序，避免显示控制台窗口。
if(APPLE)
    qt_add_executable(roco_helper_plugin MACOSX_BUNDLE
            src/main.cpp
            ${APP_SOURCES}
    )
else()
    qt_add_executable(roco_helper_plugin WIN32
            src/main.cpp
            ${APP_SOURCES}
            "${ROCO_GENERATED_DIR}/app.rc"
    )
endif()

set_target_properties(roco_helper_plugin PROPERTIES
        OUTPUT_NAME "roco_helper_plugin"
)

if(APPLE)
    set_target_properties(roco_helper_plugin PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "洛克助手插件"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.h3110w0r1d.roco_helper_plugin"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${ROCO_APP_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${ROCO_APP_VERSION}"
    )
endif()

# 嵌入不可变应用资源，例如地图、图标。
qt_add_resources(roco_helper_plugin "app_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        FILES ${APP_RESOURCE_FILES}
)

target_include_directories(roco_helper_plugin PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${ROCO_GENERATED_DIR}"
)

target_link_libraries(roco_helper_plugin PRIVATE
        Qt6::Widgets
        Qt6::Network
)

if(WIN32)
    target_link_libraries(roco_helper_plugin PRIVATE dwmapi)
endif()

target_compile_features(roco_helper_plugin PRIVATE cxx_std_17)

if(MINGW)
    target_compile_options(roco_helper_plugin PRIVATE -Wa,-mbig-obj)
endif()

# 仅 Release 启用的链接体积优化：dead_strip 去掉未引用代码，-x/-S 剥离符号表。
if(APPLE)
    target_link_options(roco_helper_plugin PRIVATE
            $<$<CONFIG:Release>:-Wl,-dead_strip>
            $<$<CONFIG:Release>:-Wl,-x>
            $<$<CONFIG:Release>:-Wl,-S>
    )
elseif(WIN32)
    if(MINGW)
        target_link_options(roco_helper_plugin PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
                $<$<CONFIG:Release>:-Wl,--gc-sections>
                $<$<CONFIG:Release>:-s>
        )
    endif()
endif()

# Windows 直接把可执行文件生成到 dist，CLion 运行 roco_helper_plugin target 时也会启动这里的 exe。
if(WIN32)
    set(ROCO_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
    set_target_properties(roco_helper_plugin PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ROCO_DIST_DIR}"
    )
    add_custom_command(TARGET roco_helper_plugin PRE_LINK
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ROCO_DIST_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ROCO_DIST_DIR}"
            VERBATIM
    )
endif()
