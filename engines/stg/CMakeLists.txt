target_sources(stg_engines PRIVATE
	detection.cpp
	font.cpp
	overlay.cpp
	resources.cpp
	screen.cpp
	sprite.cpp
	stg.cpp
)
target_compile_definitions(stg_engines PUBLIC ENABLE_STG)
