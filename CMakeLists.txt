cmake_minimum_required(VERSION 3.16)
project(rich_demo LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist)

function(richman_enable_warnings target_name)
    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target_name} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE
            -Wall
            -Wextra
            -Werror
            -pedantic
        )
    endif()
endfunction()

add_library(richman_core
    src/game.c
    src/game_mine.c
    src/game_jail.c
    src/game_toolShop.c
    src/game_property.c
    src/game_giftShop.c
    src/usr_action.c
    src/usr_judge.c
    src/file_utils.c
    src/path_utils.c
    src/case_loader.c
    src/action_executor.c
    src/actual_writer.c
    src/expected_checker.c
    src/test_runner.c
    src/manual_ui.c
    src/player_setup.c
    src/console.c
    third_party/cJSON/cJSON.c
)

target_include_directories(richman_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cJSON
)
richman_enable_warnings(richman_core)

add_executable(rich_demo src/main.c)
target_link_libraries(rich_demo PRIVATE richman_core)
richman_enable_warnings(rich_demo)

add_custom_command(
    TARGET rich_demo
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/spec/map.json"
        "$<TARGET_FILE_DIR:rich_demo>/map.json"
    COMMENT "Copy map.json next to rich_demo"
)

include(CTest)

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
