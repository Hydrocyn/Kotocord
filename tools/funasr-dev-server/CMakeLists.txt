# ==========================================
# funasr_dev_server — FunASR 协议级练习服务器 (L2-2f)
# ==========================================
add_executable(funasr_dev_server
    main.cpp
)

target_link_libraries(funasr_dev_server PRIVATE
    Qt6::Core
    Qt6::WebSockets
)

set_target_properties(funasr_dev_server PROPERTIES
    WIN32_EXECUTABLE FALSE
)
