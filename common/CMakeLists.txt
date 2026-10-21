add_library(stg_common STATIC
	archive.cpp
	config-manager.cpp
	debug.cpp
	error.cpp
	events.cpp
	file.cpp
	fs.cpp
	md5.cpp
	random.cpp
	savefile.cpp
	str.cpp
	stream.cpp
	system.cpp
	audio/audiostream.cpp
	audio/mixer.cpp
	audio/raw.cpp
	audio/wave.cpp
	graphics/palette.cpp
	graphics/pixelformat.cpp
	graphics/surface.cpp
	image/bmp.cpp
)

# Headers are included as "common/foo.h" from the repository root.
target_include_directories(stg_common PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_definitions(stg_common PUBLIC STG_VERSION="${PROJECT_VERSION}")

find_package(Threads REQUIRED)
target_link_libraries(stg_common PUBLIC Threads::Threads)

# std::filesystem needs an extra library on older GCC.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
	target_link_libraries(stg_common PUBLIC stdc++fs)
endif()
