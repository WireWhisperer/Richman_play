/**
 * @file console.c
 * @brief Terminal encoding and ANSI color setup.
 */
#include "console.h"

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

void console_init(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    {
        HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;

        if (output != INVALID_HANDLE_VALUE &&
            GetConsoleMode(output, &mode)) {
            SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
    {
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode = 0;

        if (input != INVALID_HANDLE_VALUE &&
            GetConsoleMode(input, &mode)) {
            SetConsoleMode(input, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#else
    /* POSIX terminals typically use UTF-8 and ANSI by default. */
#endif
}

void console_pause_before_exit(void)
{
    int ch;

    (void)fprintf(stderr, "\n按回车键关闭窗口...\n");
    (void)fflush(stderr);
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}
