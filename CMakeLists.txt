cmake_minimum_required(VERSION 3.16)
project(rich_demo LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

function(richman_enable_warnings target_name)
    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target_name} PRIVATE /WX /utf-8)
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
    src/player_setup.c
    src/usr_action.c
    src/usr_judge.c
    src/buy_land.c
)

target_include_directories(richman_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
richman_enable_warnings(richman_core)

add_executable(rich_demo src/main.c)
target_link_libraries(rich_demo PRIVATE richman_core)
richman_enable_warnings(rich_demo)

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
