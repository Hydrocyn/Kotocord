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
#   Qt6         — MaintenanceTool 安装, CMAKE_PREFIX_PATH / Qt6_DIR 定位
#   whisper.cpp — FetchContent 拉取源码构建 (pin v1.7.5)
#   Vosk        — third_party/ 手动管理
#
#   > 此前的 USE_VCPKG_QT / USE_VCPKG 双分支已于 2026-08-12 移除，
#     详见 LabNotebook notes/kotocord/docs/tasks/kotocord-upgrade-log.md
include(FetchContent)

# ==========================================
# 3. Qt6 (MaintenanceTool 安装, 环境变量定位)
# ==========================================
find_package(Qt6 COMPONENTS Widgets Multimedia WebSockets REQUIRED)

# ==========================================
# 4. whisper.cpp (FetchContent — 静态链接, 无 DLL 部署负担)
# ==========================================
# 关闭 whisper.cpp / ggml 的附带构建目标, 只构建库本身
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_CLI      OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_TESTS       OFF CACHE BOOL "" FORCE)
set(GGML_BUILD_EXAMPLES    OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG        v1.7.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(whisper)
message(STATUS "[Kotocord] whisper.cpp — FetchContent (v1.7.5, static)")

# ==========================================
# 5. Vosk — 手动管理 (不在 vcpkg 中)
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
    ${VOSK_DIR}/include                           # Vosk
    ${whisper_SOURCE_DIR}                         # whisper.cpp (FetchContent)
)

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
        "将 libvosk.dll 放入 ${VOSK_DIR}/lib/,\n"
        "然后运行 .\\generate-vosk-import-lib.ps1 生成 MSVC 导入库 vosk.lib.")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    whisper
    ${VOSK_LIB}
)

# ==========================================
# 10. 运行时 DLL 部署 (仅 Windows)
# ==========================================
if(WIN32)

    # --- 10.1 Vosk DLL (含 MinGW 运行时) — 始终从 third_party 拷贝 ---
    # whisper.cpp 经 FetchContent 静态链接进主程序, 无需 DLL 部署
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

# ==========================================
# 13. 开发工具 (L2: 练习服务器 + CLI 联调客户端)
# ==========================================
option(KOTOCORD_BUILD_TOOLS "Build development tools (funasr dev server / client CLI)" ON)
if(KOTOCORD_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
