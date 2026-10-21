target_sources(stg_engines PRIVATE
	detection.cpp
	disasm.cpp
	font.cpp
	opcodes.cpp
	overlay.cpp
	resources.cpp
	screen.cpp
	script.cpp
	sprite.cpp
	stg.cpp
	vm.cpp
)
target_compile_definitions(stg_engines PUBLIC ENABLE_STG)
