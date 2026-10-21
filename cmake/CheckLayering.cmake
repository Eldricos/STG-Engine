# Fails if code in common/ or engines/ refers to SDL, or common/ includes
# engine headers. Run with: cmake -DSOURCE_DIR=<repo> -P CheckLayering.cmake

file(GLOB_RECURSE lower_files
	"${SOURCE_DIR}/common/*.h" "${SOURCE_DIR}/common/*.cpp"
	"${SOURCE_DIR}/engines/*.h" "${SOURCE_DIR}/engines/*.cpp")

set(violations "")
foreach(file IN LISTS lower_files)
	file(STRINGS "${file}" lines REGEX "#[ \t]*include[ \t]*[<\"](SDL|sdl/)")
	if(lines)
		list(APPEND violations "${file}: ${lines}")
	endif()
	if(file MATCHES "/common/")
		file(STRINGS "${file}" lines REGEX "#[ \t]*include[ \t]*\"engines/")
		if(lines)
			list(APPEND violations "${file}: ${lines}")
		endif()
	endif()
endforeach()

if(violations)
	string(REPLACE ";" "\n" violations "${violations}")
	message(FATAL_ERROR "Layering violations:\n${violations}")
endif()
message(STATUS "Layering OK")
