cmake_minimum_required(VERSION 3.16)
project(Kotocord VERSION 0.1.0 LANGUAGES CXX)

# ==========================================
# 1. 编译选项与全局配置
# ==========================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ==========================================
# 2. 依赖管理模式
# ==========================================
#   USE_VCPKG_QT = ON  → Qt6 由 vcpkg 提供 (toolchain 自动设搜索路径)
#                  OFF → Qt6 通过 CMAKE_PREFIX_PATH / Qt6_DIR 查找
#   USE_VCPKG    = ON  → whisper 等其他库由 vcpkg 提供
#                  OFF → 从 third_party/ 手动加载
#
#   四种组合:
#     QT=ON  其它=ON  → 全新 PC, 全部 vcpkg
#     QT=OFF 其它=OFF → 旧 PC, 官方 Qt + third_party
#     QT=ON  其它=OFF → vcpkg 的 Qt, 但 whisper 用本地版本
#     QT=OFF 其它=ON  → 官方 Qt, 其他库交给 vcpkg
option(USE_VCPKG_QT "Use vcpkg for Qt6" ON)
option(USE_VCPKG    "Use vcpkg for whisper.cpp and other libs" ON)

# ==========================================
# 3. Qt6
# ==========================================
#   USE_VCPKG_QT=ON  → toolchain.cmake 已将 vcpkg installed 加入搜索路径
#   USE_VCPKG_QT=OFF → CMAKE_PREFIX_PATH / Qt6_DIR 指向官方安装
find_package(Qt6 COMPONENTS Widgets Multimedia REQUIRED)

# ==========================================
# 4. whisper.cpp
# ==========================================
if(USE_VCPKG)
    find_package(whisper CONFIG REQUIRED)
    set(WHISPER_LIBS whisper)
    message(STATUS "[Kotocord] whisper.cpp — vcpkg (find_package)")
else()
    # --- 手动模式: 从 third_party/whisper/ 加载 ---
    set(WHISPER_DIR "${CMAKE_SOURCE_DIR}/third_party/whisper")

    if(NOT EXISTS "${WHISPER_DIR}/include/whisper.h")
        message(FATAL_ERROR
            "whisper.h not found at ${WHISPER_DIR}/include/\n"
            "手动模式下请从 https://github.com/ggml-org/whisper.cpp/releases "
            "下载 whisper.cpp 预编译包, 解压至 third_party/whisper/")
    endif()

    # whisper 主库
    find_library(WHISPER_LIB
        NAMES whisper
        PATHS "${WHISPER_DIR}/lib"
        NO_DEFAULT_PATH REQUIRED)

    # ggml — 不同版本的 whisper.cpp 预编译包命名和布局不同:
    #   旧版: ggml.lib 单独一个文件在 ggml/lib/ 或 lib/ 下
    #   新版: 拆分为 ggml-base.lib + ggml-cpu.lib
    #   有些包: ggml 符号直接编入 whisper.lib, 不提供单独的 ggml.lib
    set(_GGML_SEARCH_DIRS "${WHISPER_DIR}/ggml/lib" "${WHISPER_DIR}/lib")
    find_library(GGML_LIB
        NAMES ggml
        PATHS ${_GGML_SEARCH_DIRS}
        NO_DEFAULT_PATH)

    if(NOT GGML_LIB)
        find_library(GGML_BASE_LIB
            NAMES ggml-base
            PATHS ${_GGML_SEARCH_DIRS}
            NO_DEFAULT_PATH)
        find_library(GGML_CPU_LIB
            NAMES ggml-cpu
            PATHS ${_GGML_SEARCH_DIRS}
            NO_DEFAULT_PATH)

        if(GGML_BASE_LIB AND GGML_CPU_LIB)
            set(GGML_LIB ${GGML_BASE_LIB} ${GGML_CPU_LIB})
            message(STATUS "[Kotocord] ggml 分体式 (ggml-base + ggml-cpu)")
        elseif(GGML_BASE_LIB)
            set(GGML_LIB ${GGML_BASE_LIB})
        endif()
    endif()

    if(GGML_LIB)
        message(STATUS "[Kotocord] ggml 独立链接: ${GGML_LIB}")
    else()
        message(STATUS "[Kotocord] 未找到独立的 ggml 导入库, "
                        "假定 ggml 符号已编入 whisper.lib (部分预编译包如此)")
    endif()

    set(WHISPER_LIBS ${WHISPER_LIB})
    if(GGML_LIB)
        list(APPEND WHISPER_LIBS ${GGML_LIB})
    endif()
    message(STATUS "[Kotocord] whisper.cpp — 手动 (third_party/whisper/)")
endif()

if(USE_VCPKG_QT)
    message(STATUS "[Kotocord] Qt6 — vcpkg")
else()
    message(STATUS "[Kotocord] Qt6 — 手动 (CMAKE_PREFIX_PATH / Qt6_DIR)")
endif()

# ==========================================
# 5. Vosk — 两种模式均手动管理 (不在 vcpkg 中)
# ==========================================
set(VOSK_DIR "${CMAKE_SOURCE_DIR}/third_party/vosk")

# ==========================================
# 6. 源码文件
# ==========================================
set(UTILS_SOURCES
    src/utils/AppPaths.h
)

set(UI_SOURCES
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.ui
)

set(RESOURCES
    resources/resources.qrc
)

set(CORE_SOURCES
    src/main.cpp
    src/core/AppController.cpp
    src/core/AppController.h
    src/core/DataTypes.h
    src/core/RAII.h
    src/core/Factory.h
)

set(MODULE_SOURCES
    # Input / ASR
    src/modules/input/IAudioTranscriber.h
    src/modules/input/VoskTranscriber.h
    src/modules/input/VoskTranscriber.cpp
    src/modules/input/WhisperTranscriber.h
    src/modules/input/WhisperTranscriber.cpp

    # Capture
    src/modules/capture/AudioCapture.h
    src/modules/capture/AudioCapture.cpp
    src/modules/capture/AudioFileSimulator.h
    src/modules/capture/AudioFileSimulator.cpp

    # LLM & Emotion
    src/modules/llm/ILanguageModel.h
    src/modules/llm/DeepSeekAPIWorker.h
    src/modules/llm/DeepSeekAPIWorker.cpp
    src/modules/llm/MockLLMWorker.h
    src/modules/llm/MockLLMWorker.cpp
    src/modules/llm/KaomojiManager.h
    src/modules/llm/KaomojiManager.cpp

    # Render
    src/modules/render/SubtitleRenderer.h
    src/modules/render/SubtitleRenderer.cpp

    # System
    src/modules/system/SystemResourceMonitor.h
    src/modules/system/SystemResourceMonitor.cpp
)

# ==========================================
# 7. 构建目标
# ==========================================
add_executable(${PROJECT_NAME}
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${MODULE_SOURCES}
    ${UTILS_SOURCES}
    ${RESOURCES}
)

# ==========================================
# 8. 头文件路径
# ==========================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${VOSK_DIR}/include                           # Vosk — 两种模式都需要
)

if(NOT USE_VCPKG)
    # 手动模式: whisper/ggml 头文件需显式声明
    # vcpkg 模式: whisper target 自带 INTERFACE_INCLUDE_DIRECTORIES, 无需手动加
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${WHISPER_DIR}/include"
        "${WHISPER_DIR}/ggml/include"
    )
endif()

# ==========================================
# 9. 链接
# ==========================================
# Vosk 导入库 (手动管理, 两种模式相同)
find_library(VOSK_LIB
    NAMES vosk libvosk
    PATHS "${VOSK_DIR}/lib"
    NO_DEFAULT_PATH
)
if(NOT VOSK_LIB)
    message(FATAL_ERROR
        "Vosk library not found in ${VOSK_DIR}/lib.\n"
        "请从 https://github.com/alphacep/vosk-api/releases 下载 vosk-win64 预编译包,\n"
        "将 libvosk.dll 放入 ${VOSK_DIR}/lib/ 并生成 MSVC 导入库 vosk.lib.\n"
        "详见 README.md → 部署方式 A 步骤 3.")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    ${WHISPER_LIBS}
    ${VOSK_LIB}
)

# ==========================================
# 10. 运行时 DLL 部署 (仅 Windows)
# ==========================================
if(WIN32)

    # --- 10.1 Vosk DLL (含 MinGW 运行时) — 始终从 third_party 拷贝 ---
    if(EXISTS "${VOSK_DIR}/lib")
        file(GLOB _VOSK_DLLS "${VOSK_DIR}/lib/*.dll")
        if(_VOSK_DLLS)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_VOSK_DLLS}
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "[Kotocord] Deploying Vosk DLLs..."
            )
        endif()
    endif()

    # --- 10.2 定位 vcpkg installed 目录 (当任一库使用 vcpkg 时) ---
    if(USE_VCPKG_QT OR USE_VCPKG)
        foreach(_prefix ${CMAKE_PREFIX_PATH})
            if(EXISTS "${_prefix}/bin" AND EXISTS "${_prefix}/include")
                set(_VCPKG_INSTALLED "${_prefix}")
                break()
            endif()
        endforeach()

        set(_RUNTIME_BIN_DIR "${_VCPKG_INSTALLED}/bin")
        set(_DEBUG_BIN_DIR   "${_VCPKG_INSTALLED}/debug/bin")
        set(_RUNTIME_PLUGINS "${_VCPKG_INSTALLED}/Qt6/plugins")
        set(_DEBUG_PLUGINS   "${_VCPKG_INSTALLED}/debug/Qt6/plugins")
    endif()

    # --- 10.3 whisper / ggml DLL ---
    if(USE_VCPKG)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/whisper.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/ggml.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/ggml-cpu.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<IF:$<CONFIG:Debug>,${_DEBUG_BIN_DIR},${_RUNTIME_BIN_DIR}>/ggml-base.dll"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "[Kotocord] Deploying whisper/ggml DLLs (vcpkg)..."
        )
    else()
        file(GLOB _WHISPER_DLLS "${WHISPER_DIR}/lib/*.dll")
        file(GLOB _GGML_DLLS   "${WHISPER_DIR}/ggml/lib/*.dll")
        if(_WHISPER_DLLS OR _GGML_DLLS)
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${_WHISPER_DLLS} ${_GGML_DLLS}
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "[Kotocord] Deploying whisper/ggml DLLs (manual third_party)..."
            )
        endif()
    endif()

    # --- 10.4 Qt 平台插件 ---
    if(USE_VCPKG_QT)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/platforms"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/styles"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/styles"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/imageformats"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/imageformats"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/tls"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/tls"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "$<IF:$<CONFIG:Debug>,${_DEBUG_PLUGINS},${_RUNTIME_PLUGINS}>/multimedia"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/multimedia"
            COMMENT "[Kotocord] Deploying Qt plugin directories (vcpkg)..."
        )
    else()
        message(STATUS "[Kotocord] Qt 手动模式: Qt 插件请用 windeployqt 或 Qt Creator 部署")
    endif()

elseif(APPLE)
    file(GLOB _VOSK_DYLIBS "${VOSK_DIR}/lib/*.dylib")
    if(_VOSK_DYLIBS)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_VOSK_DYLIBS}
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        )
    endif()
else()
    file(GLOB _VOSK_SOS "${VOSK_DIR}/lib/*.so")
    if(_VOSK_SOS)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_VOSK_SOS}
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        )
    endif()
endif()

# ==========================================
# 11. 预编译头 (PCH)
# ==========================================
target_precompile_headers(${PROJECT_NAME} PRIVATE
    <QString>
    <QObject>
    <QWidget>
    <QDebug>
    <QMutex>
    <QThread>
    <QSettings>
    <QTimer>
    <QJsonDocument>
    <QNetworkAccessManager>
    <QNetworkReply>
)

# ==========================================
# 12. 单元测试
# ==========================================
enable_testing()
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
