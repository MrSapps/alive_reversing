# Plays converted game data through the real sound code, offline, and saves traces and WAVs to
# compare before and after a sound change. See Source/relive_lib/Sound/SOUND_FORMATS.md.
add_executable(relive_sound_gold
    Main.cpp)

target_include_directories(relive_sound_gold PRIVATE ${CMAKE_BINARY_DIR})
target_compile_definitions(relive_sound_gold PRIVATE "_CRT_SECURE_NO_WARNINGS")
target_link_libraries(relive_sound_gold relive_lib relive_ipc project_warnings SDL3::SDL3)
set_target_properties(relive_sound_gold PROPERTIES FOLDER "Tools")

if (MSVC)
    target_compile_options(relive_sound_gold PRIVATE /W4 /wd4996 /WX /MP)

    if (CMAKE_GENERATOR MATCHES "Visual Studio")
        add_custom_command(TARGET relive_sound_gold POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE_DIR:relive_sound_gold>")
    endif()
endif()
