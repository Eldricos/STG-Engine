if(STG_VENDORED_SDL)
	include(FetchContent)
	FetchContent_Declare(SDL3
		GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
		GIT_TAG release-3.2.24
		GIT_SHALLOW TRUE)
	set(SDL_TESTS OFF CACHE BOOL "" FORCE)
	set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(SDL3)
else()
	find_package(SDL3 3.2 REQUIRED CONFIG COMPONENTS SDL3)
endif()

add_executable(stg MACOSX_BUNDLE
	main.cpp
	sdl-events.cpp
	sdl-graphics.cpp
	sdl-mixer.cpp
	sdl-system.cpp
)
target_link_libraries(stg PRIVATE stg_engines SDL3::SDL3)

if(WIN32 AND NOT STG_VENDORED_SDL)
	# Copy SDL3.dll next to the executable so it runs from the build tree.
	add_custom_command(TARGET stg POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:stg>)
endif()

install(TARGETS stg RUNTIME DESTINATION bin BUNDLE DESTINATION .)
