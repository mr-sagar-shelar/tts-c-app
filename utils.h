#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>

/* Key Definitions */
#define KEY_UP 1001
#define KEY_DOWN 1002
#define KEY_ENTER 1003
#define KEY_ESC 1004
#define KEY_BACKSPACE 127
#define KEY_TAB 9
#define KEY_CTRL_I 9
#define KEY_CTRL_E 5
#define KEY_CTRL_P 16
#define KEY_CTRL_L 12

/**
 * Initializes a UTF-8 locale for the process when possible.
 * Returns 1 when a UTF-8 locale is active, 0 otherwise.
 */
int init_utf8_locale();

/**
 * Re-asserts UTF-8 mode on terminals that support the standard escape.
 */
void enable_utf8_terminal_mode();

/**
 * Sets the terminal to non-canonical mode for raw input.
 */
void set_conio_terminal_mode();

/**
 * Resets the terminal to its original mode.
 */
void reset_terminal_mode();

/**
 * Reads a single key press, handling escape sequences for arrow keys.
 * @return The key code or character read.
 */
int read_key();

/**
 * Reads a single key press with an optional timeout.
 * @param timeout_ms The timeout in milliseconds. Use 0 for immediate poll.
 * @return The key code, 0 on timeout, or character read.
 */
int read_key_timeout(int timeout_ms);

/**
 * Gets user input from stdin, temporarily resetting terminal mode.
 * @param buffer The buffer to store input.
 * @param size The size of the buffer.
 * @param prompt The prompt to display.
 */
void get_user_input(char *buffer, int size, const char *prompt);

/**
 * URL encodes a string.
 * @param str The string to encode.
 * @return A newly allocated encoded string (must be freed).
 */
char *url_encode(const char *str);

/**
 * Displays a list of numbers for the user to pick from.
 * @param title The title of the picker.
 * @param min The minimum value.
 * @param max The maximum value.
 * @param current The current value.
 * @return The selected value or -1 on escape.
 */
int handle_value_picker(const char *title, int min, int max, int current);

/**
 * Moves a menu selection with wrap-around behavior.
 * @param current The current index.
 * @param direction Use -1 for up and +1 for down.
 * @param count The total number of items.
 * @return The next wrapped index.
 */
int menu_next_index(int current, int direction, int count);

/**
 * Returns the terminal size, falling back to 24x80 when unavailable.
 * @param rows Output row count.
 * @param cols Output column count.
 */
void get_terminal_size(int *rows, int *cols);

/**
 * Reads current memory usage in megabytes when supported.
 * @param used_mb Output used memory in MB.
 * @param total_mb Output total memory in MB.
 * @return 1 on success, 0 when unavailable.
 */
int get_memory_usage_mb(unsigned long *used_mb, unsigned long *total_mb);

/**
 * Prints blank lines so a footer can remain pinned near the bottom.
 * @param used_lines The number of lines already printed.
 * @param footer_lines The number of lines reserved for the footer.
 */
void pad_screen_to_footer(int used_lines, int footer_lines);

#endif /* UTILS_H */
