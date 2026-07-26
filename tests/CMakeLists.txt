# ==========================================
# Kotocord 单元测试
# ==========================================
find_package(Qt6 COMPONENTS Test REQUIRED)

# --- 共享模块源码 (被多个测试复用) ---
# 因为它们使用相对 include (如 "../../core/DataTypes.h"),
# 编译时以源文件自身目录为起点解析, 无需额外处理.

# ==========================================
# Test 1: AppController — 核心调度逻辑
# ==========================================
# AppController 依赖 KaomojiManager、ILanguageModel (MOC)
# WIN32 不加在测试目标上 — Qt Test 需要控制台输出
add_executable(test_controller
    TestAppController.cpp
    ../src/core/AppController.cpp
    ../src/core/AppController.h
    ../src/core/DataTypes.h
    ../src/modules/llm/ILanguageModel.h        # 为 MockLLMForTest 的 MOC
    ../src/modules/llm/KaomojiManager.cpp      # AppController 依赖
    ../src/modules/llm/KaomojiManager.h
)

target_include_directories(test_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_controller PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Widgets
)

add_test(NAME ControllerTests COMMAND $<TARGET_FILE:test_controller>)


# ==========================================
# Test 2: KaomojiManager — 颜文字加载与查询
# ==========================================
add_executable(test_kaomoji
    TestKaomojiManager.cpp
    ../src/modules/llm/KaomojiManager.cpp
    ../src/modules/llm/KaomojiManager.h
    ../src/core/DataTypes.h
)

target_include_directories(test_kaomoji PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_kaomoji PRIVATE
    Qt6::Test
    Qt6::Core
)

add_test(NAME KaomojiTests COMMAND $<TARGET_FILE:test_kaomoji>)


# ==========================================
# Test 0: Sanity — 验证 Qt Test 框架正常
# ==========================================
add_executable(test_sanity
    SanityTest.cpp
)

target_link_libraries(test_sanity PRIVATE
    Qt6::Test
    Qt6::Core
)

add_test(NAME SanityTest COMMAND $<TARGET_FILE:test_sanity>)

# 确保测试可执行文件在 Windows 上使用控制台子系统
set_target_properties(test_controller test_kaomoji test_sanity PROPERTIES
    WIN32_EXECUTABLE FALSE
)

# Qt6 on Windows: 需要显式设置环境变量才能将输出发送到 stderr
set_tests_properties(ControllerTests KaomojiTests SanityTest PROPERTIES
    ENVIRONMENT "QT_ASSUME_STDERR_HAS_CONSOLE=1"
)
