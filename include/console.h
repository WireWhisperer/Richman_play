/**
 * @file console.h
 * @brief Terminal encoding and ANSI color setup.
 */
#ifndef RICH_CONSOLE_H
#define RICH_CONSOLE_H

/** Enable UTF-8 and ANSI colors on the current terminal. */
void console_init(void);

/** Wait for Enter before closing the console window. */
void console_pause_before_exit(void);

#endif /* RICH_CONSOLE_H */
