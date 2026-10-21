# Use an installed SDL3 if there is one, otherwise download and build it.
set(use_vendored_sdl ${STG_VENDORED_SDL})
if(NOT use_vendored_sdl)
	find_package(SDL3 3.2 QUIET CONFIG COMPONENTS SDL3)
	if(SDL3_FOUND)
		message(STATUS "Using installed SDL3 ${SDL3_VERSION}: ${SDL3_DIR}")
	elseif(STG_DOWNLOAD_SDL)
		message(STATUS "SDL3 not found; it will be downloaded and built (set SDL3_DIR or CMAKE_PREFIX_PATH to use an installed copy)")
		set(use_vendored_sdl ON)
	else()
		message(FATAL_ERROR "SDL3 3.2 or newer was not found. Install its development package, point "
		                    "CMAKE_PREFIX_PATH or SDL3_DIR at it, or enable STG_DOWNLOAD_SDL.")
	endif()
endif()

if(use_vendored_sdl)
	include(FetchContent)
	FetchContent_Declare(SDL3
		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
		GIT_TAG release-3.2.24
		GIT_SHALLOW TRUE)
	# Link statically so stg runs without an SDL3 shared library next to it.
	set(SDL_SHARED OFF CACHE BOOL "" FORCE)
	set(SDL_STATIC ON CACHE BOOL "" FORCE)
	set(SDL_TESTS OFF CACHE BOOL "" FORCE)
	set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(SDL3)
endif()

add_executable(stg MACOSX_BUNDLE
	main.cpp
	sdl-events.cpp
	sdl-graphics.cpp
	sdl-mixer.cpp
	sdl-system.cpp
)
target_link_libraries(stg PRIVATE stg_engines SDL3::SDL3)

if(WIN32 AND NOT use_vendored_sdl)
	# Copy SDL3.dll next to the executable so it runs from the build tree.
	add_custom_command(TARGET stg POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:stg>)
endif()

install(TARGETS stg RUNTIME DESTINATION bin BUNDLE DESTINATION .)
