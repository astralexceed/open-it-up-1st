# Builds the web version, the desktop version (-DPIU_BUILD_DESKTOP=ON) and the tests.

cmake_minimum_required(VERSION 3.20)

option(PIU_BUILD_DESKTOP "Build the desktop game (downloads SDL2 and libchdr)" OFF)
if(PIU_BUILD_DESKTOP)
  # Older than macOS 11 there are no Apple Silicon machines to run it on.
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(open_it_up C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keeps the folder where the game is built out of the programs it produces.
if(NOT MSVC)
  add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX,OBJC,OBJCXX>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>")
endif()

add_library(piu_core STATIC
  src/core/assets.cpp
  src/core/attract.cpp
  src/core/bga_player.cpp
  src/core/credit_hud.cpp
  src/core/game.cpp
  src/core/json.cpp
  src/core/key_map.cpp
  src/core/key_map_screen.cpp
  src/core/results.cpp
  src/core/scene.cpp
  src/core/select_screen.cpp
  src/core/service_menu.cpp
  src/core/song_data.cpp
  src/core/stage_cfg.cpp)
target_include_directories(piu_core PUBLIC src)

add_library(piu_formats STATIC src/extractor/formats.cpp)
target_include_directories(piu_formats PUBLIC src)

# The extractor reads the user's CHD. libchdr is BSD-3 and is downloaded while
# building, pinned to this exact version.
if(EMSCRIPTEN OR PIU_BUILD_DESKTOP)
  include(FetchContent)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(CHDR_WANT_TESTS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(libchdr
    GIT_REPOSITORY https://github.com/rtissera/libchdr.git
    GIT_TAG 47d794151b2b64f6aba60304e6480f4f5a2bc24b)
  FetchContent_MakeAvailable(libchdr)

  add_library(piu_cd STATIC src/extractor/cd_image.cpp src/extractor/extractor.cpp)
  target_link_libraries(piu_cd PUBLIC piu_formats chdr-static)
endif()

if(EMSCRIPTEN)
  add_executable(piu src/platform/web/main_web.cpp src/platform/engine_api.cpp src/render/gl_renderer.cpp)
  target_link_libraries(piu PRIVATE piu_core)
  set_target_properties(piu PROPERTIES
    SUFFIX ".html"
    LINK_DEPENDS "${CMAKE_SOURCE_DIR}/web/shell.html")
  target_link_options(piu PRIVATE
    "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
    "-sMIN_WEBGL_VERSION=2"
    "-sMAX_WEBGL_VERSION=2"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sENVIRONMENT=web"
    "-sEXPORTED_RUNTIME_METHODS=HEAPU8,UTF8ToString")

  add_executable(piu_extract src/platform/web/extract_web.cpp)
  target_link_libraries(piu_extract PRIVATE piu_cd)
  target_link_options(piu_extract PRIVATE
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createExtractor"
    "-sENVIRONMENT=worker"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sMAXIMUM_MEMORY=2147483648"
    "-sEXPORTED_RUNTIME_METHODS=HEAPU8,UTF8ToString")
  configure_file(web/extract_worker.js extract_worker.js COPYONLY)
else()
  enable_testing()
  add_executable(core_test tests/core_test.cpp)
  target_link_libraries(core_test PRIVATE piu_core piu_formats)
  add_test(NAME core_test COMMAND core_test)
endif()

if(PIU_BUILD_DESKTOP)
  # SDL2 is zlib-licensed and is linked into the game, pinned to release 2.32.8.
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG 98d1f3a45aae568ccd6ed5fec179330f47d4d356)
  FetchContent_MakeAvailable(SDL2)
  find_package(OpenGL REQUIRED)

  set(PIU_DESKTOP_SOURCES
    src/platform/desktop/audio.cpp
    src/platform/desktop/game_app.cpp
    src/platform/desktop/library.cpp
    src/platform/desktop/main_desktop.cpp
    src/platform/engine_api.cpp
    src/render/gl_renderer.cpp)
  if(APPLE)
    set_source_files_properties(src/platform/desktop/launcher_mac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set_source_files_properties(assets/icons/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(open_it_up MACOSX_BUNDLE ${PIU_DESKTOP_SOURCES} src/platform/desktop/launcher_mac.mm assets/icons/icon.icns)
    set_target_properties(open_it_up PROPERTIES
      OUTPUT_NAME "Open It UP"
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/src/platform/desktop/Info.plist.in")
    target_link_libraries(open_it_up PRIVATE "-framework Cocoa")
  elseif(WIN32)
    enable_language(RC)
    add_executable(open_it_up WIN32 ${PIU_DESKTOP_SOURCES} src/platform/desktop/launcher_win32.cpp
      src/render/gl_loader_win32.cpp assets/icons/app.rc)
    set_target_properties(open_it_up PROPERTIES OUTPUT_NAME "Open It UP")
    target_compile_definitions(open_it_up PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(open_it_up PRIVATE comctl32 comdlg32)
    if(MSVC)
      target_compile_options(open_it_up PRIVATE /utf-8)
      target_link_options(open_it_up PRIVATE /ENTRY:mainCRTStartup /MANIFEST:NO)
    else()
      target_link_options(open_it_up PRIVATE -static -static-libgcc -static-libstdc++ -s)
    endif()
  else()
    add_executable(open_it_up ${PIU_DESKTOP_SOURCES} src/platform/desktop/launcher_stub.cpp)
  endif()
  target_link_libraries(open_it_up PRIVATE piu_core piu_formats piu_cd SDL2::SDL2-static OpenGL::GL)
endif()
