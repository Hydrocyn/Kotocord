# ==========================================
# funasr_client_cli — FunASR CLI 联调客户端 (L2-2g)
# ==========================================
add_executable(funasr_client_cli
    main.cpp
    ../../src/modules/input/FunASRTranscriber.cpp
    ../../src/modules/input/FunASRTranscriber.h
    ../../src/modules/input/FunASRSocketWorker.cpp
    ../../src/modules/input/FunASRSocketWorker.h
    ../../src/modules/input/IAudioTranscriber.h
    ../../src/modules/capture/AudioFileSimulator.cpp
    ../../src/modules/capture/AudioFileSimulator.h
)

target_include_directories(funasr_client_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(funasr_client_cli PRIVATE
    Qt6::Core
    Qt6::WebSockets
)

set_target_properties(funasr_client_cli PROPERTIES
    WIN32_EXECUTABLE FALSE
)
