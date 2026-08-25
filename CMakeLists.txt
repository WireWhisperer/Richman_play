cmake_minimum_required(VERSION 3.10)
project(Richman_Play)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(Richman_Play
    src/main.c
    src/game.c
    src/file_utils.c
    src/case_loader.c
    src/action_executor.c
    src/actual_writer.c
    src/expected_checker.c
    src/test_runner.c
    src/manual_ui.c
)
