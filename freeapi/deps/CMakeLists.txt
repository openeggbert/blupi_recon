cmake_minimum_required(VERSION 3.24)
project(BLUPI_WEB_SDL LANGUAGES C CXX)
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "Configure these dependencies with emcmake")
endif()
include(FetchContent)
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
add_compile_options(-pthread)
add_link_options(-pthread)
set_property(GLOBAL PROPERTY JOB_POOLS compile_pool=4)
set(CMAKE_JOB_POOL_COMPILE compile_pool)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL ON CACHE BOOL "" FORCE)
set(SDLIMAGE_INSTALL ON CACHE BOOL "" FORCE)
set(SDLMIXER_INSTALL ON CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
# Only the builtin BMP loader is needed by the game; no optional codec deps.
foreach(codec AVIF JXL TIF WEBP PNG JPG)
    set(SDLIMAGE_${codec} OFF CACHE BOOL "" FORCE)
endforeach()
set(SDLMIXER_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "" FORCE)
# MIDI uses Free API's TinySoundFont; effects use Free Direct's PCM streams.
foreach(codec AIFF AU FLAC GME MOD MP3 MIDI OPUS VOC VORBIS WAVPACK)
    set(SDLMIXER_${codec} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(sdl3
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-3.4.0
    URL_HASH SHA256=9614b9696abc4597ffce6b888829dc6537ae500423474c342ac4a67222c5654c)
FetchContent_Declare(sdl3_image
    URL https://codeload.github.com/libsdl-org/SDL_image/tar.gz/refs/tags/release-3.4.0
    URL_HASH SHA256=5d541520e5a5a12cb6854336df434af64c7bd72ec08cca6c880d8b411a0139a7)
FetchContent_Declare(sdl3_mixer
    URL https://codeload.github.com/libsdl-org/SDL_mixer/tar.gz/refs/tags/release-3.2.0
    URL_HASH SHA256=1077fef4a6e9513027da0a08e6f1b8645f8c88c8bef001c17befeb52b129b5dd)
FetchContent_MakeAvailable(sdl3 sdl3_image sdl3_mixer)
