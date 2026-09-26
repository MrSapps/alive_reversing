# Draws test scenes with the real renderers and engine code, without any game data.
# See README.md.
add_executable(relive_render_test
    Main.cpp
    RenderTest.cpp
    RenderTest.hpp
    Scene.cpp
    Scene.hpp
    Scenes.cpp
    TestResources.cpp
    TestResources.hpp
    Capture.cpp
    Capture.hpp
    README.md)

target_include_directories(relive_render_test PRIVATE ${CMAKE_BINARY_DIR})
target_compile_definitions(relive_render_test PRIVATE "_CRT_SECURE_NO_WARNINGS")
target_link_libraries(relive_render_test relive_lib relive_ipc project_warnings SDL3::SDL3)
set_target_properties(relive_render_test PROPERTIES FOLDER "Tools")

if (MSVC)
    target_compile_options(relive_render_test PRIVATE /W4 /wd4996 /WX /MP)

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        add_custom_command(TARGET relive_render_test POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE_DIR:relive_render_test>")
    endif()
endif()

# Every scene on both renderers, with no window on screen: the stability, object, texture and
# state leak checks must pass. Few timed frames, as this is for correctness, not timing.
# SDL's software renderer can't do the blend modes the SDL3 renderer needs, so SDL's OpenGL one
# is used. ASan's leak check is off: Mesa's offscreen (EGL) drivers are unloaded before it runs,
# so their own allocations look leaked. Run on a desktop to leak check it, see README.md.
if (NOT WIN32 AND NOT APPLE)
    add_test(NAME relive_render_test
             COMMAND relive_render_test -auto -renderer=all -frames=5 -out=${CMAKE_CURRENT_BINARY_DIR}/render_test_out)
    set_tests_properties(relive_render_test PROPERTIES
        ENVIRONMENT "SDL_VIDEO_DRIVER=offscreen;SDL_RENDER_DRIVER=opengl;ASAN_OPTIONS=detect_leaks=0")
endif()
