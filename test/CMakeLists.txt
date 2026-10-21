add_executable(stg_tests
	test_main.cpp
	null-system.cpp
	test_audio.cpp
	test_common.cpp
	test_engines.cpp
	test_graphics.cpp
	test_level.cpp
	test_script.cpp
	test_zip.cpp
)
target_link_libraries(stg_tests PRIVATE stg_engines)
add_test(NAME unit COMMAND stg_tests)
