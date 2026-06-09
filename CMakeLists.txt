cmake_minimum_required(VERSION 3.27)

project(roco_helper LANGUAGES CXX)

# 项目依赖：Qt 用于应用和界面，protobuf、PcapPlusPlus
# 由 rwtd 抓包/解码层使用。
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql HttpServer Network)
find_package(Protobuf CONFIG REQUIRED)
find_package(PcapPlusPlus REQUIRED CONFIG)

# 启用 Qt 代码生成，并尽量减少导出的符号。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# 将 rwtd 源码放到独立目标中，避免大量 protobuf/抓包代码混入界面源码列表。
file(GLOB_RECURSE RWTD_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/*.cpp"
)

# 应用源码包含 src 下除 rwtd 外的其他源码；rwtd 通过下面的静态库链接进来。
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER APP_SOURCES EXCLUDE REGEX ".*/src/rwtd/.*")

file(GLOB_RECURSE APP_RESOURCE_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/*"
)

# 运行时流量抓包/解码库。
add_library(rwtd STATIC
        ${RWTD_SOURCES}
)

target_include_directories(rwtd PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rwtd/proto"
)

target_link_libraries(rwtd
        PUBLIC
        Qt6::Core
        protobuf::libprotobuf
        PRIVATE
        PcapPlusPlus::Pcap++
)

if(WIN32)
    # 使用 GetAdaptersAddresses() 将 Npcap 设备 GUID 映射为可读名称。
    target_link_libraries(rwtd PRIVATE iphlpapi)
elseif(APPLE)
    # 使用 SystemConfiguration 将 en0 这类 BSD 设备名映射为可读服务名。
    target_link_libraries(rwtd PRIVATE
            "-framework CoreFoundation"
            "-framework SystemConfiguration"
    )
endif()

target_compile_features(rwtd PUBLIC cxx_std_17)

if(MINGW)
    target_compile_options(rwtd PRIVATE -Wa,-mbig-obj)
endif()

# macOS 构建为 .app bundle；Windows 构建为 GUI 子系统程序，避免显示控制台窗口。
if(APPLE)
    qt_add_executable(roco_helper MACOSX_BUNDLE
            src/main.cpp
            ${APP_SOURCES}
    )
else()
    qt_add_executable(roco_helper WIN32
            src/main.cpp
            ${APP_SOURCES}
            resources/windows/app.rc
    )
endif()

# 嵌入不可变应用资源，例如地图、图标和 rwtd schema。
qt_add_resources(roco_helper "app_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        FILES ${APP_RESOURCE_FILES}
        OPTIONS -no-compress
)

target_include_directories(roco_helper PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(roco_helper PRIVATE
        Qt6::Widgets
        Qt6::Sql
        Qt6::HttpServer
        Qt6::Network
        rwtd
)

# 静态 Qt 会自动导入 SQL 插件；本项目只使用 SQLite，排除 ODBC 可避免额外链接系统 ODBC。
if(TARGET Qt6::QODBCDriverPlugin)
    qt_import_plugins(roco_helper
            EXCLUDE Qt6::QODBCDriverPlugin
    )
endif()

target_compile_features(roco_helper PRIVATE cxx_std_17)

# 仅 Release 启用的链接体积优化。
if(APPLE)
    target_link_options(roco_helper PRIVATE
            $<$<CONFIG:Release>:-Wl,-dead_strip>
    )
elseif(WIN32)
    if(MINGW)
        target_link_options(roco_helper PRIVATE
                -static
                -static-libgcc
                -static-libstdc++
                $<$<CONFIG:Release>:-Wl,--gc-sections>
                $<$<CONFIG:Release>:-s>
        )
    endif()
endif()

# Windows 直接把可执行文件生成到 dist，CLion 运行 roco_helper target 时也会启动这里的 exe。
if(WIN32)
    set(ROCO_DIST_DIR "${CMAKE_BINARY_DIR}/dist")
    set_target_properties(roco_helper PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ROCO_DIST_DIR}"
    )
    add_custom_command(TARGET roco_helper PRE_LINK
            COMMAND "${CMAKE_COMMAND}" -E rm -rf "${ROCO_DIST_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ROCO_DIST_DIR}"
            VERBATIM
    )
endif()
