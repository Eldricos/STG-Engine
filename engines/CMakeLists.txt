add_library(stg_engines STATIC
	detection.cpp
	engine.cpp
	game.cpp
	launcher.cpp
	metaengine.cpp
)
target_link_libraries(stg_engines PUBLIC stg_common)

# Each engine lives in its own subdirectory and adds its sources to
# stg_engines, plus an ENABLE_<NAME> definition so metaengine.cpp
# registers it.
if(STG_ENABLE_STG)
	add_subdirectory(stg)
endif()
