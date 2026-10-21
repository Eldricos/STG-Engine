target_sources(stg_engines PRIVATE
	detection.cpp
	font.cpp
	resources.cpp
	screen.cpp
	stg.cpp
)
target_compile_definitions(stg_engines PUBLIC ENABLE_STG)
