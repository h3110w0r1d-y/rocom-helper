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

if(MSVC)
    # 生成的 protobuf 源码较大，MSVC 需要放宽对象文件大小限制。
    target_compile_options(rwtd PRIVATE /bigobj)
elseif(MINGW)
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
    )
endif()

# 嵌入不可变应用资源，例如地图、图标和 rwtd schema。
qt_add_resources(roco_helper "app_resources"
        PREFIX "/"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
        FILES ${APP_RESOURCE_FILES}
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

target_compile_options(roco_helper PRIVATE
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
        $<$<AND:$<CONFIG:Release>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-O2>
)
target_compile_features(roco_helper PRIVATE cxx_std_17)

# 仅 Release 启用的链接体积优化。
if(APPLE)
    target_link_options(roco_helper PRIVATE
            $<$<CONFIG:Release>:-Wl,-dead_strip>
    )
elseif(WIN32)
    if(MSVC)
        target_link_options(roco_helper PRIVATE
                $<$<CONFIG:Release>:/OPT:REF>
                $<$<CONFIG:Release>:/OPT:ICF>
        )
    else()
        target_link_options(roco_helper PRIVATE
                $<$<CONFIG:Release>:-Wl,--gc-sections>
                $<$<CONFIG:Release>:-s>
        )
    endif()
endif()

if(MSVC)
    # 大部分大型生成源码位于 rwtd，因此这里也保留 /bigobj。
    target_compile_options(rwtd PRIVATE /bigobj)
endif()

set(ROCO_DIST_DIR "${CMAKE_BINARY_DIR}/dist")

# 平台部署：
# - macOS 保留 .app bundle，并由 macdeployqt 修正 Qt frameworks/plugins。
# - Windows 输出扁平 dist 目录，包含 exe、运行时 DLL 和 Qt 插件。
if(APPLE)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(MACDEPLOYQT_EXECUTABLE
            NAMES macdeployqt
            HINTS "${QT_BIN_DIR}"
            REQUIRED
    )

    add_custom_command(TARGET roco_helper POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:roco_helper>"
            VERBATIM
    )
elseif(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE
            NAMES windeployqt
            HINTS "${QT_BIN_DIR}"
            REQUIRED
    )

    add_custom_command(TARGET roco_helper POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${ROCO_DIST_DIR}"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:roco_helper>"
                    "${ROCO_DIST_DIR}/$<TARGET_FILE_NAME:roco_helper>"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:roco_helper>
                    "${ROCO_DIST_DIR}"
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    "$<$<CONFIG:Debug>:--debug>$<$<NOT:$<CONFIG:Debug>>:--release>"
                    --compiler-runtime
                    --dir "${ROCO_DIST_DIR}"
                    "${ROCO_DIST_DIR}/$<TARGET_FILE_NAME:roco_helper>"
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
endif()
