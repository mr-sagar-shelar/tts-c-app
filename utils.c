#include "utils.h"
#include "menu.h"

#include <langinfo.h>
#include <locale.h>

static struct termios original_termios;
static int is_conio_mode = 0;

static int locale_codeset_is_utf8() {
    const char *codeset = nl_langinfo(CODESET);

    return codeset &&
           (strcmp(codeset, "UTF-8") == 0 || strcmp(codeset, "UTF8") == 0);
}

int init_utf8_locale() {
    static const char *fallback_locales[] = {
        "",
        "C.UTF-8",
        "en_US.UTF-8",
        "hi_IN.UTF-8"
    };
    size_t i;

    for (i = 0; i < sizeof(fallback_locales) / sizeof(fallback_locales[0]); i++) {
        if (!setlocale(LC_ALL, fallback_locales[i])) {
            continue;
        }
        if (locale_codeset_is_utf8()) {
            return 1;
        }
    }

    return 0;
}

void enable_utf8_terminal_mode() {
    if (!isatty(STDOUT_FILENO)) {
        return;
    }

    /* Select UTF-8 character set on terminals that understand ISO-2022 escapes. */
    fputs("\033%G", stdout);
    fflush(stdout);
}

static int read_key_internal(int timeout_ms) {
    char c;

    if (timeout_ms >= 0) {
        struct termios work_termios;
        struct termios tmp_termios;

        tcgetattr(STDIN_FILENO, &work_termios);
        tmp_termios = work_termios;
        tmp_termios.c_cc[VMIN] = 0;
        tmp_termios.c_cc[VTIME] = (cc_t)((timeout_ms + 99) / 100);
        tcsetattr(STDIN_FILENO, TCSANOW, &tmp_termios);

        if (read(STDIN_FILENO, &c, 1) != 1) {
            tcsetattr(STDIN_FILENO, TCSANOW, &work_termios);
            return 0;
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &work_termios);
    } else {
        if (read(STDIN_FILENO, &c, 1) != 1) {
            return 0;
        }
    }

    if (c == 27) { // Escape or start of sequence
        struct termios work_termios;
        tcgetattr(STDIN_FILENO, &work_termios);
        struct termios tmp_termios = work_termios;
        tmp_termios.c_cc[VMIN] = 0;
        tmp_termios.c_cc[VTIME] = 1; // 100ms timeout
        tcsetattr(STDIN_FILENO, TCSANOW, &tmp_termios);

        char seq[2];
        int n = read(STDIN_FILENO, &seq[0], 1);
        if (n <= 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &work_termios);
            return KEY_ESC;
        }
        n = read(STDIN_FILENO, &seq[1], 1);

        tcsetattr(STDIN_FILENO, TCSANOW, &work_termios);

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
            }
        }
        return KEY_ESC;
    } else if (c == 10 || c == 13) {
        return KEY_ENTER;
    } else if (c == 127 || c == 8) {
        return KEY_BACKSPACE;
    } else if (c == 9) {
        return KEY_TAB;
    }
    return (unsigned char)c;
}

void reset_terminal_mode() {
    if (is_conio_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        is_conio_mode = 0;
    }
}

void set_conio_terminal_mode() {
    if (is_conio_mode) return;
    
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(reset_terminal_mode);
    
    new_termios = original_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    is_conio_mode = 1;
}

int read_key() {
    return read_key_internal(-1);
}

int read_key_timeout(int timeout_ms) {
    return read_key_internal(timeout_ms);
}

void get_user_input(char *buffer, int size, const char *prompt) {
    reset_terminal_mode();
    printf("\n%s: ", prompt);
    if (fgets(buffer, size, stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
    }
    set_conio_terminal_mode();
}

char *url_encode(const char *str) {
    static const char *hex = "0123456789ABCDEF";
    size_t len = strlen(str);
    char *encoded = (char*)malloc(len * 3 + 1);
    if (!encoded) return NULL;

    char *p = encoded;
    while (*str) {
        unsigned char c = (unsigned char)*str;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            *p++ = c;
        } else {
            *p++ = '%';
            *p++ = hex[c >> 4];
            *p++ = hex[c & 15];
        }
        str++;
    }
    *p = '\0';
    return encoded;
}

int handle_value_picker(const char *title, int min, int max, int current) {
    int sel = current - min;
    int count = max - min + 1;
    int PAGE_SIZE = 15;
    int scroll = (sel / PAGE_SIZE) * PAGE_SIZE;

    while (1) {
        printf("\033[H\033[J--- %s ---\n", title);
        int end = scroll + PAGE_SIZE;
        if (end > count) end = count;

        for (int i = scroll; i < end; i++) {
            if (i == sel) printf("> %02d\n", i + min);
            else printf("  %02d\n", i + min);
        }
        printf("\n%s\n", menu_translate("ui_footer_back", "[Arrows: Navigate | Enter: Select | Esc: Back]"));
        fflush(stdout);

        int key = read_key();
        if (key == KEY_UP) {
            sel = menu_next_index(sel, -1, count);
            if (sel < scroll) scroll = (sel / PAGE_SIZE) * PAGE_SIZE;
            else if (sel >= scroll + PAGE_SIZE) scroll = (sel / PAGE_SIZE) * PAGE_SIZE;
        } else if (key == KEY_DOWN) {
            sel = menu_next_index(sel, 1, count);
            if (sel >= scroll + PAGE_SIZE) scroll = (sel / PAGE_SIZE) * PAGE_SIZE;
            else if (sel < scroll) scroll = (sel / PAGE_SIZE) * PAGE_SIZE;
        } else if (key == KEY_ENTER) {
            return sel + min;
        } else if (key == KEY_ESC) {
            return -1;
        }
    }
}

int menu_next_index(int current, int direction, int count) {
    if (count <= 0) {
        return 0;
    }

    current += direction;
    if (current < 0) {
        return count - 1;
    }
    if (current >= count) {
        return 0;
    }
    return current;
}
