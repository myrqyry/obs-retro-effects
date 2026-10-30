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

# Authentic H.263 and MPEG-4 Part 2 encode/decode (opt-in).
option(ENABLE_VIDEO_CODECS "Enable actual H.263/MPEG-4 Part 2 codecs" OFF)
option(ENABLE_H263_CODEC "Legacy alias for ENABLE_VIDEO_CODECS" OFF)
if(ENABLE_H263_CODEC)
  set(ENABLE_VIDEO_CODECS ON)
endif()
if(ENABLE_VIDEO_CODECS)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(VIDEO_FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil libswscale)
  target_sources(${PROJECT_NAME} PRIVATE
    src/codec/video-roundtrip.c src/codec/video-roundtrip.h
    src/codec/video-obs.c src/codec/video-obs.h)
  target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::VIDEO_FFMPEG)
  target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_VIDEO_CODECS=1)

  include(CTest)
  if(BUILD_TESTING)
    add_executable(test_h263_roundtrip tests/test-h263-roundtrip.c src/codec/video-roundtrip.c)
    target_link_libraries(test_h263_roundtrip PRIVATE PkgConfig::VIDEO_FFMPEG)
    add_test(NAME h263_roundtrip COMMAND test_h263_roundtrip)

    add_executable(test_video_codecs tests/test-video-codecs.c src/codec/video-roundtrip.c)
    target_link_libraries(test_video_codecs PRIVATE PkgConfig::VIDEO_FFMPEG)
    add_test(NAME video_codecs_roundtrip COMMAND test_video_codecs)

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      find_program(XVFB_RUN xvfb-run)
      if(XVFB_RUN)
        add_executable(test_h263_obs tests/test-h263-obs.cpp)
        target_link_libraries(test_h263_obs PRIVATE OBS::libobs)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/h263-test-home")
        foreach(format h263 mpeg4)
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
    src/filters/posterize.h
    src/filters/vhs.c
    src/filters/vhs.h
    src/filters/scan-lines.c
    src/filters/scan-lines.h
    src/version.h
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
