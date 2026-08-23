add_executable(test_us03 test_us03.c)
target_link_libraries(test_us03 PRIVATE richman_core)
richman_enable_warnings(test_us03)

add_test(NAME US03_player_setup COMMAND test_us03)
