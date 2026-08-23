add_executable(test_us03 test_us03.c)
target_link_libraries(test_us03 PRIVATE richman_core)
richman_enable_warnings(test_us03)

# Release 配置默认定义 NDEBUG 会禁用 assert，测试必须保持断言生效。
if(MSVC)
    target_compile_options(test_us03 PRIVATE /U NDEBUG)
else()
    target_compile_options(test_us03 PRIVATE -UNDEBUG)
endif()

add_test(NAME US03_player_setup COMMAND test_us03)
