cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
target_sources(${PROJECT_NAME} PRIVATE src/filters/dither-diffusion.h
 src/filters/dither-cpu-render.c src/filters/dither-cpu-render.h
 src/filters/vhs-osd.c src/filters/vhs-osd.h)

# Experimental drop-in kernel. Linux-only until target/ABI packaging is qualified.
# OFF preserves all upstream C behavior and cross-platform build workflows.
option(ENABLE_RUST_DITHER "Use the experimental Rust CPU error-diffusion kernel (Linux only)" OFF)
if(ENABLE_RUST_DITHER)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "ENABLE_RUST_DITHER is currently supported only on Linux")
  endif()
  find_program(RUSTC_EXECUTABLE rustc REQUIRED)
  find_package(Threads REQUIRED)
  set(RETRO_RUST_DITHER_LIB "${CMAKE_CURRENT_BINARY_DIR}/libretro_dither_rs.a")
  add_custom_command(
    OUTPUT "${RETRO_RUST_DITHER_LIB}"
    COMMAND "${RUSTC_EXECUTABLE}" --edition=2021 --crate-type=staticlib
      -C opt-level=3 -C panic=abort -C relocation-model=pic
      -o "${RETRO_RUST_DITHER_LIB}"
      "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/dither_diffusion.rs"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/rust/dither_diffusion.rs"
    VERBATIM)
  add_custom_target(retro_rust_dither_build DEPENDS "${RETRO_RUST_DITHER_LIB}")
  add_library(retro_rust_dither STATIC IMPORTED GLOBAL)
  set_target_properties(retro_rust_dither PROPERTIES IMPORTED_LOCATION "${RETRO_RUST_DITHER_LIB}")
  add_dependencies(retro_rust_dither retro_rust_dither_build)
  target_sources(${PROJECT_NAME} PRIVATE src/filters/dither-rust-shim.c)
  target_link_libraries(${PROJECT_NAME} PRIVATE retro_rust_dither ${CMAKE_DL_LIBS} Threads::Threads m)
else()
  target_sources(${PROJECT_NAME} PRIVATE src/filters/dither-diffusion.c)
endif()

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME retro_foundation_source_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-retro-foundation-contract.py)
    add_test(NAME matrix_rain_source_contract
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-matrix-rain-contract.py)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import PIL"
      RESULT_VARIABLE MATRIX_RAIN_PILLOW_AVAILABLE OUTPUT_QUIET ERROR_QUIET)
    if(MATRIX_RAIN_PILLOW_AVAILABLE EQUAL 0)
      add_test(NAME matrix_rain_glyph_atlas
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-matrix-glyph-atlas.py)
    endif()
  endif()
  add_executable(test_dither_diffusion tests/test-dither-diffusion.c src/filters/dither-diffusion.c)
  if(UNIX)
    target_link_libraries(test_dither_diffusion PRIVATE m)
  endif()
  add_test(NAME dither_diffusion COMMAND test_dither_diffusion)
  if(ENABLE_RUST_DITHER)
    add_executable(test_dither_rust_parity tests/test-dither-rust-parity.c src/filters/dither-diffusion.c)
    target_compile_definitions(test_dither_rust_parity PRIVATE _POSIX_C_SOURCE=200809L)
    target_link_libraries(test_dither_rust_parity PRIVATE retro_rust_dither ${CMAKE_DL_LIBS} Threads::Threads m)
    add_test(NAME dither_rust_parity COMMAND test_dither_rust_parity)
  endif()
  add_executable(test_retro_palette tests/test-retro-palette.c src/filters/retro-palette.c)
  add_test(NAME retro_palette_lut COMMAND test_retro_palette)
  add_executable(test_blue_noise_mask tests/test-blue-noise-mask.c)
  add_test(NAME blue_noise_mask COMMAND test_blue_noise_mask)
  add_executable(test_vhs_osd tests/test-vhs-osd.c src/filters/vhs-osd.c)
  add_test(NAME vhs_osd_unit COMMAND test_vhs_osd)

endif()


if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_program(XVFB_RUN xvfb-run)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/h263-test-home")
  if(XVFB_RUN)
        add_executable(test_effect_hotkeys tests/test-effect-hotkeys.cpp)
        target_link_libraries(test_effect_hotkeys PRIVATE OBS::libobs)
        add_test(NAME effect_hotkeys_obs
          COMMAND ${CMAKE_COMMAND} -E env
            "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            ${XVFB_RUN} -a
            $<TARGET_FILE:test_effect_hotkeys>
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_CURRENT_SOURCE_DIR}/data" obs_retro_effects_filter)
        set_tests_properties(effect_hotkeys_obs PROPERTIES TIMEOUT 30)
        add_executable(test_dither_ui tests/test-dither-ui.cpp)
        target_link_libraries(test_dither_ui PRIVATE OBS::libobs)
        add_test(NAME dither_ui
          COMMAND ${CMAKE_COMMAND} -E env
            "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            ${XVFB_RUN} -a
            $<TARGET_FILE:test_dither_ui>
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_CURRENT_SOURCE_DIR}/data")
        set_tests_properties(dither_ui PROPERTIES TIMEOUT 30)
        add_executable(test_dither_obs tests/test-dither-obs.cpp)
        target_link_libraries(test_dither_obs PRIVATE OBS::libobs)
        add_test(NAME dither_obs_render
          COMMAND ${CMAKE_COMMAND} -E env
            "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            ${XVFB_RUN} -a
            $<TARGET_FILE:test_dither_obs>
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_CURRENT_SOURCE_DIR}/data")
        set_tests_properties(dither_obs_render PROPERTIES TIMEOUT 45)
        add_executable(test_matrix_rain_obs tests/test-matrix-rain-obs.cpp)
        target_link_libraries(test_matrix_rain_obs PRIVATE OBS::libobs)
        add_test(NAME matrix_rain_obs_render
          COMMAND ${CMAKE_COMMAND} -E env
            "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            ${XVFB_RUN} -a
            $<TARGET_FILE:test_matrix_rain_obs>
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_CURRENT_SOURCE_DIR}/data")
        set_tests_properties(matrix_rain_obs_render PROPERTIES TIMEOUT 45)
        add_executable(test_vhs_obs tests/test-vhs-obs.cpp)
        target_link_libraries(test_vhs_obs PRIVATE OBS::libobs)
        add_test(NAME vhs_osd_obs_render
          COMMAND ${CMAKE_COMMAND} -E env
            "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            ${XVFB_RUN} -a
            $<TARGET_FILE:test_vhs_obs>
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_CURRENT_SOURCE_DIR}/data")
        set_tests_properties(vhs_osd_obs_render PROPERTIES TIMEOUT 45)
        add_executable(test_retro_foundation_obs tests/test-retro-foundation-obs.cpp)
        target_link_libraries(test_retro_foundation_obs PRIVATE OBS::libobs)
        add_test(NAME retro_foundation_obs_render
          COMMAND ${CMAKE_COMMAND} -E env
            "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            ${XVFB_RUN} -a
            $<TARGET_FILE:test_retro_foundation_obs>
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_CURRENT_SOURCE_DIR}/data")
        set_tests_properties(retro_foundation_obs_render PROPERTIES TIMEOUT 45)
  endif()
endif()

# Authentic H.263 and MPEG-4 Part 2 encode/decode (opt-in).
option(ENABLE_VIDEO_CODECS "Enable the eight-codec FFmpeg encoder/decoder gallery" OFF)
option(ENABLE_H263_CODEC "Legacy alias for ENABLE_VIDEO_CODECS" OFF)
if(ENABLE_H263_CODEC)
  set(ENABLE_VIDEO_CODECS ON)
endif()
if(ENABLE_VIDEO_CODECS)
  include(RetroFFmpeg)
  find_package(Threads REQUIRED)
  target_sources(${PROJECT_NAME} PRIVATE
    src/codec/video-roundtrip.c src/codec/video-roundtrip.h
    src/codec/video-obs.c src/codec/video-obs.h
    src/codec/codec-worker.cpp src/codec/codec-worker.h)
  target_link_libraries(${PROJECT_NAME} PRIVATE retro_ffmpeg Threads::Threads)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_VIDEO_CODECS=1)

  include(CTest)
  if(BUILD_TESTING)
    add_executable(test_codec_worker tests/test-codec-worker.cpp src/codec/codec-worker.cpp src/codec/video-roundtrip.c)
    target_link_libraries(test_codec_worker PRIVATE retro_ffmpeg Threads::Threads)
    add_test(NAME codec_worker COMMAND test_codec_worker)

    add_executable(test_h263_roundtrip tests/test-h263-roundtrip.c src/codec/video-roundtrip.c)
    target_link_libraries(test_h263_roundtrip PRIVATE retro_ffmpeg)
    add_test(NAME h263_roundtrip COMMAND test_h263_roundtrip)

    add_executable(test_video_codecs tests/test-video-codecs.c src/codec/video-roundtrip.c)
    target_link_libraries(test_video_codecs PRIVATE retro_ffmpeg)
    add_test(NAME video_codecs_roundtrip COMMAND test_video_codecs)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      find_program(XVFB_RUN xvfb-run)
      if(XVFB_RUN)
        add_executable(test_h263_obs tests/test-h263-obs.cpp)
        target_link_libraries(test_h263_obs PRIVATE OBS::libobs)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/h263-test-home")
        foreach(format h263 mpeg4 cinepak svq1 msvideo1 flv1 mpeg1 mjpeg)
          add_test(
            NAME ${format}_obs_render
            COMMAND ${CMAKE_COMMAND} -E env
              "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
              "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
              ${XVFB_RUN} -a
              $<TARGET_FILE:test_h263_obs>
              $<TARGET_FILE:${PROJECT_NAME}>
              "${CMAKE_CURRENT_SOURCE_DIR}/data"
              ${format}
          )
          set_tests_properties(${format}_obs_render PROPERTIES TIMEOUT 40)
        endforeach()
        add_test(NAME codec_portrait_render
          COMMAND ${CMAKE_COMMAND} -E env
            "HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            "XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/h263-test-home"
            ${XVFB_RUN} -a
            $<TARGET_FILE:test_h263_obs>
            $<TARGET_FILE:${PROJECT_NAME}>
            "${CMAKE_CURRENT_SOURCE_DIR}/data" mpeg4 portrait)
        set_tests_properties(codec_portrait_render PROPERTIES TIMEOUT 40)
      endif()
    endif()
  endif()
endif()

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${PROJECT_NAME}
  PRIVATE
    src/base-filter.h
    src/obs-retro-effects-filter.c
    src/obs-retro-effects-plugin.c
    src/obs-retro-effects-filter.h
    src/obs-retro-effects.h
    src/obs-utils.c
    src/obs-utils.h
    src/blur/bloom.c
    src/blur/bloom.h
    src/blur/blur.c
    src/blur/blur.h
    src/blur/gaussian-kernel.c
    src/blur/gaussian-kernel.h
    src/filters/analog-glitch.c
    src/filters/analog-glitch.h
    src/filters/bloom-f.c
    src/filters/bloom-f.h
    src/filters/cathode-boot.c
    src/filters/cathode-boot.h
    src/filters/chromatic-aberration.c
    src/filters/chromatic-aberration.h
    src/filters/crt.c
    src/filters/crt.h
    src/filters/codec.c
    src/filters/codec.h
    src/filters/digital-glitch.c
    src/filters/digital-glitch.h
    src/filters/dither.c
    src/filters/dither.h
    src/filters/frame-skip.c
    src/filters/frame-skip.h
    src/filters/interlace.c
    src/filters/interlace.h
    src/filters/matrix-rain.c
    src/filters/matrix-rain.h
    src/filters/ntsc.c
    src/filters/ntsc.h
    src/filters/posterize.c
    src/filters/retro-palette.c
    src/filters/retro-palette.h
    src/filters/retro-palette-gpu.c
    src/filters/retro-palette-gpu.h
    src/filters/posterize.h
    src/filters/vhs.c
    src/filters/vhs.h
    src/filters/scan-lines.c
    src/filters/scan-lines.h
    src/version.h
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
