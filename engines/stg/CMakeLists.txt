target_sources(stg_engines PRIVATE
	actors.cpp
	detection.cpp
	disasm.cpp
	font.cpp
	level.cpp
	mission.cpp
	opcodes.cpp
	overlay.cpp
	hud.cpp
	projectiles.cpp
	render3d.cpp
	resources.cpp
	screen.cpp
	script.cpp
	sound.cpp
	sprite.cpp
	stg.cpp
	vm.cpp
	walker.cpp
)
target_compile_definitions(stg_engines PUBLIC ENABLE_STG)
