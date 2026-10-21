cmake_minimum_required(VERSION 3.16)

project(STGEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(STG_ENABLE_STG "Build the Star Trek: Generations engine" ON)
option(STG_BUILD_TESTS "Build the unit tests" ON)
option(STG_VENDORED_SDL "Download and build SDL3 instead of using an installed copy" OFF)

if(MSVC)
	add_compile_options(/W4 /wd4100 /wd4244 /wd4267 /D_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Layering, as in ScummVM:
#
#   sdl/      the platform backend. The only code that sees SDL.
#   engines/  the launcher, engine framework and the game engines.
#   common/   platform independent services: OSystem interface, files,
#             streams, graphics surfaces, audio mixer, events, config.
#
# Dependencies only point downwards: sdl -> engines -> common. common and
# engines are built without SDL's include paths so a stray SDL include
# fails to compile, and the 'layering' test catches the rest.

add_subdirectory(common)
add_subdirectory(engines)
add_subdirectory(sdl)

if(STG_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
	add_test(NAME layering
	         COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/CheckLayering.cmake)
endif()
