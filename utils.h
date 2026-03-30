#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

/* Key Definitions */
#define KEY_UP 1001
#define KEY_DOWN 1002
#define KEY_ENTER 1003
#define KEY_ESC 1004
#define KEY_BACKSPACE 127
#define KEY_TAB 9
#define KEY_CTRL_I 9

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

#endif /* UTILS_H */
