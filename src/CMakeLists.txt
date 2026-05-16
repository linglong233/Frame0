# Testable logic library (no DX11 dependency)
add_library(reaction_timer_lib STATIC
    Timer.cpp
    Config.cpp
    Storage.cpp
    TestSession.cpp
)

target_include_directories(reaction_timer_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(reaction_timer_lib
    PUBLIC nlohmann_json::nlohmann_json
)

# Application executable
add_executable(ReactionTimer
    main.cpp
    Renderer.cpp
    Input.cpp
    UI.cpp
)

set_target_properties(ReactionTimer PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(ReactionTimer
    PRIVATE reaction_timer_lib
    PRIVATE d3d11 dxgi d2d1 dwrite user32 gdi32
)
