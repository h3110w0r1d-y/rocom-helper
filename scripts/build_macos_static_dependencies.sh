#!/usr/bin/env bash
set -euo pipefail

# 备份脚本：静态编译插件项目需要的 macOS 依赖。
# 用法：
#   ./scripts/build_macos_static_dependencies.sh qt
#   ./scripts/build_macos_static_dependencies.sh all
#
# 生成后的项目 CMAKE_PREFIX_PATH：
#   /Users/yuyang/CLionProjects/libs/qt-6.11.1-macos-static

LIBS_ROOT="${LIBS_ROOT:-/Users/yuyang/CLionProjects/libs}"
BUILD_ROOT="${BUILD_ROOT:-${LIBS_ROOT}/build}"

QT_SRC="${QT_SRC:-${LIBS_ROOT}/qt-6.11.1}"
QT_BUILD="${QT_BUILD:-${BUILD_ROOT}/qt-6.11.1-macos-static}"
QT_INSTALL="${QT_INSTALL:-${LIBS_ROOT}/qt-6.11.1-macos-static}"

JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"

build_qt() {
    rm -rf "${QT_BUILD}" "${QT_INSTALL}"
    mkdir -p "${QT_BUILD}"

    (
        cd "${QT_BUILD}"
        "${QT_SRC}/configure" \
            -prefix "${QT_INSTALL}" \
            -static \
            -release \
            -opensource \
            -confirm-license \
            -nomake examples \
            -nomake tests \
            -securetransport \
            -no-openssl \
            -qt-zlib \
            -qt-pcre \
            -qt-libpng \
            -qt-libjpeg \
            -qt-freetype \
            -qt-harfbuzz \
            -qt-sqlite \
            -skip qtwebengine \
            -skip qtmultimedia \
            -skip qtquick3d \
            -skip qt3d \
            -skip qtcharts \
            -skip qtlocation \
            -skip qtpositioning \
            -skip qtconnectivity \
            -skip qtsensors \
            -skip qtserialport \
            -skip qtserialbus \
            -skip qtwayland \
            -skip qtquick3dphysics \
            -skip qtspeech \
            -skip qtgrpc \
            -skip qtdoc \
            -skip qtmqtt \
            -skip qtnetworkauth \
            -skip qtwebview \
            -skip qtvirtualkeyboard \
            -skip qt5compat \
            -skip qtdeclarative \
            -skip qtimageformats \
            -skip qtlanguageserver \
            -skip qtshadertools \
            -skip qtactiveqt \
            -skip qtcanvaspainter \
            -skip qtcoap \
            -skip qtdatavis3d \
            -skip qtgraphs \
            -skip qttasktree \
            -skip qtlottie \
            -skip qtopcua \
            -skip qtopenapi \
            -skip qtquickeffectmaker \
            -skip qtquicktimeline \
            -skip qtremoteobjects \
            -skip qtscxml \
            -skip qtwebchannel \
            -skip qttools \
            -skip qtsvg \
            -skip qttranslations
    )

    cmake --build "${QT_BUILD}" --parallel "${JOBS}"
    cmake --install "${QT_BUILD}"
}

print_prefix_path() {
    echo
    echo "CMAKE_PREFIX_PATH=\"${QT_INSTALL}\""
}

case "${1:-}" in
    qt)
        build_qt
        print_prefix_path
        ;;
    all)
        build_qt
        print_prefix_path
        ;;
    *)
        sed -n '2,12p' "$0"
        exit 2
        ;;
esac
