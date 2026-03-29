#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios original_termios;

void reset_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void set_conio_terminal_mode() {
    struct termios new_termios;

    /* take current terminal settings and copy to original_termios */
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(reset_terminal_mode);
    new_termios = original_termios;

    /* disable canonical mode (buffered i/o) and local echo */
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
}

int main() {
    char c;

    set_conio_terminal_mode();

    printf("Program started. Type anything to see it printed. Press Escape to exit.\n\n");

    while (1) {
        if (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 27) { // Escape key
                printf("\nEscape key pressed. Exiting...\n");
                break;
            }

            // For printable characters, just print them
            // We print it and flush because we're in non-canonical mode
            printf("You pressed: %c (ASCII: %d)\n", (c >= 32 && c <= 126) ? c : '?', (unsigned char)c);
            fflush(stdout);
        }
    }

    return 0;
}
