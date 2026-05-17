# Testable logic library (no DX11 dependency)
add_library(frame0_lib STATIC
    Timer.cpp
    Config.cpp
    Storage.cpp
    TestSession.cpp
    FullscreenRecovery.cpp
)

target_include_directories(frame0_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(frame0_lib
    PUBLIC nlohmann_json::nlohmann_json
)

# Application executable
add_executable(Frame0
    main.cpp
    Renderer.cpp
    Input.cpp
    UI.cpp
)

set_target_properties(Frame0 PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(Frame0
    PRIVATE frame0_lib
    PRIVATE d3d11 dxgi d2d1 dwrite user32 gdi32
)
