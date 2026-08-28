# ==========================================
# tts_client_cli — TTS CLI 联调客户端 (L2-3g, D9 起驱动 Python 侧车引擎)
# ==========================================
add_executable(tts_client_cli
    main.cpp
    ../../src/modules/tts/PythonEdgeTTS.cpp
    ../../src/modules/tts/PythonEdgeTTS.h
    ../../src/modules/tts/ITextToSpeech.h
    ../../src/modules/tts/TTSPlayer.cpp
    ../../src/modules/tts/TTSPlayer.h
    ../../src/utils/AppPaths.h
)

target_include_directories(tts_client_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(tts_client_cli PRIVATE
    Qt6::Core
    Qt6::Multimedia
)

set_target_properties(tts_client_cli PROPERTIES
    WIN32_EXECUTABLE FALSE
)
