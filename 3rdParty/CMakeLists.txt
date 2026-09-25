project(Deps VERSION 0.1 LANGUAGES C CXX)


set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(ENABLE_DOCS OFF)
set(ENABLE_EXAMPLES OFF)
set(ENABLE_TESTS OFF)
set(ENABLE_TOOLS OFF)
set(CONFIG_ANALYZER OFF)
set(CONFIG_INSPECTION OFF)
set(BUILD_SHARED_LIBS OFF)
set(CONFIG_WEBM_IO ON)
set(ENABLE_TOOLS ON) # required to get webm project
add_definitions(-DNOMINMAX)
set(AOM_TARGET_CPU generic)

# aom's configure step rewrites its generated rtcd headers and dummy sources on every CMake
# run, even when their content is unchanged. The new mtime makes ~260 aom files recompile.
# Stash the previous copies, then put back any that are unchanged: file(COPY) keeps the
# original timestamp.
set(aom_bin_dir "${CMAKE_CURRENT_BINARY_DIR}/aom")
set(aom_stash_dir "${CMAKE_CURRENT_BINARY_DIR}/aom_generated_stash")
file(GLOB aom_generated_files RELATIVE "${aom_bin_dir}"
     "${aom_bin_dir}/config/*_rtcd.h" "${aom_bin_dir}/gen_src/*")
file(REMOVE_RECURSE "${aom_stash_dir}")
foreach(f ${aom_generated_files})
    get_filename_component(f_dir "${f}" DIRECTORY)
    file(COPY "${aom_bin_dir}/${f}" DESTINATION "${aom_stash_dir}/${f_dir}")
endforeach()

add_subdirectory(aom EXCLUDE_FROM_ALL)

foreach(f ${aom_generated_files})
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${aom_stash_dir}/${f}" "${aom_bin_dir}/${f}"
                    RESULT_VARIABLE aom_file_differs)
    if(aom_file_differs EQUAL 0)
        get_filename_component(f_dir "${f}" DIRECTORY)
        file(COPY "${aom_stash_dir}/${f}" DESTINATION "${aom_bin_dir}/${f_dir}")
    endif()
endforeach()
set_property(TARGET aom PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_av1_common PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_av1_decoder PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_av1_encoder PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_dsp PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_dsp_common PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_dsp_decoder PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_dsp_encoder PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_mem PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_pc PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_rtcd PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_scale PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_util PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_version PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_version_check PROPERTY FOLDER "3rdParty")
set_property(TARGET av1_rc_qmode PROPERTY FOLDER "3rdParty")
set_property(TARGET dist PROPERTY FOLDER "3rdParty")
set_property(TARGET yuv PROPERTY FOLDER "3rdParty")
set_property(TARGET webm PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_common_app_util PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_decoder_app_util PROPERTY FOLDER "3rdParty")
set_property(TARGET aom_encoder_app_util PROPERTY FOLDER "3rdParty")
export(TARGETS aom webm aom_common_app_util FILE aom.cmake)


# Cinder for lock free circular buffer in audio processing
add_library(Cinder INTERFACE)
target_include_directories(Cinder SYSTEM INTERFACE Cinder/)
export(TARGETS Cinder FILE Cinder.cmake)

# googletest lib
set(googletest_src
    googletest/googletest/src/gtest-all.cc
    googletest/googlemock/src/gmock-all.cc
    )

add_library(googletest ${googletest_src})

target_include_directories(googletest SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest/include/gtest>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest/include/gtest/internal>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock/internal>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock/internal/custom>"
    "$<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock>"
    )

export(TARGETS googletest FILE googletest.cmake)
set_property(TARGET googletest PROPERTY FOLDER "3rdParty")

# googletest lib dynamic crt
add_library(googletest_dynamic_crt ${googletest_src})

target_include_directories(googletest_dynamic_crt SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest/include/gtest>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googletest/include/gtest/internal>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock/internal>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock/internal/custom>"
    "$<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/googletest/googlemock/include/gmock>"
    )

set_property(TARGET googletest_dynamic_crt PROPERTY MSVC_RUNTIME_LIBRARY ${RELIVE_DYNAMIC_CRT})
export(TARGETS googletest_dynamic_crt FILE googletest_dynamic_crt.cmake)
set_property(TARGET googletest_dynamic_crt PROPERTY FOLDER "3rdParty")

# dear imgui lib
add_library(imgui
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl3.cpp
    imgui/backends/imgui_impl_opengl3.cpp
    )

target_link_libraries(imgui
    libglew_static
    SDL3::SDL3
    )

target_include_directories(imgui SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends>"
    )
add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLEW)
export(TARGETS imgui FILE imgui.cmake)
set_property(TARGET imgui PROPERTY FOLDER "3rdParty")

option(glew-cmake_BUILD_SHARED "" OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/glew-cmake EXCLUDE_FROM_ALL)
set_property(TARGET libglew_static PROPERTY FOLDER "3rdParty")
export(TARGETS libglew_static FILE libglew_static.cmake)

# glm
add_library(glm INTERFACE)

target_include_directories(glm SYSTEM INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glm>"
    )
export(TARGETS glm FILE glm.cmake)

# miniz lib
set(miniz_src
    miniz/miniz.c
    miniz/miniz.h
    )

add_library(miniz ${miniz_src})

target_include_directories(miniz SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/miniz>"
    "$<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/miniz>"
    )

export(TARGETS miniz FILE miniz.cmake)
set_property(TARGET miniz PROPERTY FOLDER "3rdParty")

# miniz lib dynamic crt
add_library(miniz_dynamic_crt ${miniz_src})

target_include_directories(miniz_dynamic_crt SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/miniz>"
    "$<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/miniz>"
    )
set_property(TARGET miniz_dynamic_crt PROPERTY MSVC_RUNTIME_LIBRARY ${RELIVE_DYNAMIC_CRT})
export(TARGETS miniz_dynamic_crt FILE miniz_dynamic_crt.cmake)
set_property(TARGET miniz_dynamic_crt PROPERTY FOLDER "3rdParty")


# spng lib
set(spng_src
    libspng/spng/spng.c
    libspng/spng/spng.h
    )

add_library(spng ${spng_src})

target_compile_definitions(spng PRIVATE SPNG_USE_MINIZ)
target_compile_definitions(spng PRIVATE SPNG_STATIC)
target_compile_definitions(spng PUBLIC SPNG_STATIC)

target_include_directories(spng SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libspng>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/miniz>"
    "$<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libspng>"
    )

export(TARGETS spng FILE spng.cmake)
set_property(TARGET spng PROPERTY FOLDER "3rdParty")

# spng lib dynamic crt
add_library(spng_dynamic_crt ${spng_src})

target_compile_definitions(spng_dynamic_crt PRIVATE SPNG_USE_MINIZ)
target_compile_definitions(spng_dynamic_crt PRIVATE SPNG_STATIC)
target_compile_definitions(spng_dynamic_crt PUBLIC SPNG_STATIC)

target_include_directories(spng_dynamic_crt SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libspng>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/miniz>"
    "$<INSTALL_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/libspng>"
    )
set_property(TARGET spng_dynamic_crt PROPERTY MSVC_RUNTIME_LIBRARY ${RELIVE_DYNAMIC_CRT})
export(TARGETS spng_dynamic_crt FILE spng_dynamic_crt.cmake)
set_property(TARGET spng_dynamic_crt PROPERTY FOLDER "3rdParty")

add_library(shelf-pack-cpp INTERFACE)
target_include_directories(shelf-pack-cpp SYSTEM INTERFACE shelf-pack-cpp/include)
export(TARGETS shelf-pack-cpp FILE shelf-pack-cpp.cmake)

# libogg + libvorbis/vorbisenc - used to encode (data conversion) and decode (playback)
# FMV audio as Ogg Vorbis instead of raw PCM.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
set(INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
add_subdirectory(ogg EXCLUDE_FROM_ALL)
add_subdirectory(vorbis EXCLUDE_FROM_ALL)
set_property(TARGET ogg PROPERTY FOLDER "3rdParty")
set_property(TARGET vorbis PROPERTY FOLDER "3rdParty")
set_property(TARGET vorbisenc PROPERTY FOLDER "3rdParty")
set_property(TARGET vorbisfile PROPERTY FOLDER "3rdParty")

# vorbisfile + the vorbis decoder + ogg in one lib, dynamic crt - for the editor's About dialog
# music. The sources mirror vorbis/lib/CMakeLists.txt and ogg/CMakeLists.txt (encoder excluded).
add_library(vorbisfile_dynamic_crt
    ogg/src/bitwise.c
    ogg/src/framing.c
    vorbis/lib/mdct.c
    vorbis/lib/smallft.c
    vorbis/lib/block.c
    vorbis/lib/envelope.c
    vorbis/lib/window.c
    vorbis/lib/lsp.c
    vorbis/lib/lpc.c
    vorbis/lib/analysis.c
    vorbis/lib/synthesis.c
    vorbis/lib/psy.c
    vorbis/lib/info.c
    vorbis/lib/floor1.c
    vorbis/lib/floor0.c
    vorbis/lib/res0.c
    vorbis/lib/mapping0.c
    vorbis/lib/registry.c
    vorbis/lib/codebook.c
    vorbis/lib/sharedbook.c
    vorbis/lib/lookup.c
    vorbis/lib/bitrate.c
    vorbis/lib/vorbisfile.c
    )
target_include_directories(vorbisfile_dynamic_crt SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ogg/include>"
    # ogg's generated config_types.h
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/ogg/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/vorbis/include>"
    )
target_include_directories(vorbisfile_dynamic_crt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vorbis/lib")
if (MSVC)
    target_compile_definitions(vorbisfile_dynamic_crt PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE _CRT_NONSTDC_NO_DEPRECATE)
elseif (UNIX)
    target_link_libraries(vorbisfile_dynamic_crt PUBLIC m)
endif()
set_property(TARGET vorbisfile_dynamic_crt PROPERTY MSVC_RUNTIME_LIBRARY ${RELIVE_DYNAMIC_CRT})
set_property(TARGET vorbisfile_dynamic_crt PROPERTY FOLDER "3rdParty")
