# Runs relive with an -automation script from the game data directory. Skips (exit 77) if there's no
# game data. Pass RELIVE, DATA_DIR and SCRIPT.
if (NOT DATA_DIR OR NOT EXISTS "${DATA_DIR}/relive_data")
    message("No converted game data in '${DATA_DIR}': skipping")
    return()
endif()
set(ENV{SDL_VIDEO_DRIVER} offscreen)
set(ENV{SDL_AUDIO_DRIVER} dummy)
set(ENV{ASAN_OPTIONS} detect_leaks=0)
execute_process(COMMAND "${RELIVE}" "-automation=${SCRIPT}" ${GAME_ARGS}
    WORKING_DIRECTORY "${DATA_DIR}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
if (NOT result EQUAL 0)
    string(REGEX MATCHALL "[^\n]*\\[automation\\][^\n]*" lines "${out}")
    list(JOIN lines "\n" lines)
    message(FATAL_ERROR "relive exited with ${result}:\n${lines}")
endif()
