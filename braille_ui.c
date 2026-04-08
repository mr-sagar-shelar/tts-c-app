#include "braille_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "menu.h"

#define BRAILLE_DEFAULT_CELLS 20
#define BRAILLE_MIN_CELLS 1
#define BRAILLE_MAX_CELLS 80
#define BRAILLE_SIZE_SMALL "small"
#define BRAILLE_SIZE_MEDIUM "medium"
#define BRAILLE_SIZE_LARGE "large"

#define DOT1 0x01
#define DOT2 0x02
#define DOT3 0x04
#define DOT4 0x08
#define DOT5 0x10
#define DOT6 0x20
#define DOT7 0x40
#define DOT8 0x80

static unsigned char braille_pattern_for_latin_letter(char lower) {
    switch (lower) {
        case 'a': return DOT1;
        case 'b': return DOT1 | DOT2;
        case 'c': return DOT1 | DOT4;
        case 'd': return DOT1 | DOT4 | DOT5;
        case 'e': return DOT1 | DOT5;
        case 'f': return DOT1 | DOT2 | DOT4;
        case 'g': return DOT1 | DOT2 | DOT4 | DOT5;
        case 'h': return DOT1 | DOT2 | DOT5;
        case 'i': return DOT2 | DOT4;
        case 'j': return DOT2 | DOT4 | DOT5;
        case 'k': return DOT1 | DOT3;
        case 'l': return DOT1 | DOT2 | DOT3;
        case 'm': return DOT1 | DOT3 | DOT4;
        case 'n': return DOT1 | DOT3 | DOT4 | DOT5;
        case 'o': return DOT1 | DOT3 | DOT5;
        case 'p': return DOT1 | DOT2 | DOT3 | DOT4;
        case 'q': return DOT1 | DOT2 | DOT3 | DOT4 | DOT5;
        case 'r': return DOT1 | DOT2 | DOT3 | DOT5;
        case 's': return DOT2 | DOT3 | DOT4;
        case 't': return DOT2 | DOT3 | DOT4 | DOT5;
        case 'u': return DOT1 | DOT3 | DOT6;
        case 'v': return DOT1 | DOT2 | DOT3 | DOT6;
        case 'w': return DOT2 | DOT4 | DOT5 | DOT6;
        case 'x': return DOT1 | DOT3 | DOT4 | DOT6;
        case 'y': return DOT1 | DOT3 | DOT4 | DOT5 | DOT6;
        case 'z': return DOT1 | DOT3 | DOT5 | DOT6;
        default: return 0;
    }
}

static unsigned char braille_pattern_for_ascii(unsigned int codepoint) {
    if (codepoint == ' ') {
        return 0;
    }

    if (codepoint >= 'a' && codepoint <= 'z') {
        return braille_pattern_for_latin_letter((char)codepoint);
    }

    if (codepoint >= 'A' && codepoint <= 'Z') {
        return braille_pattern_for_latin_letter((char)(codepoint - 'A' + 'a')) | DOT7;
    }

    switch (codepoint) {
        case '1': return braille_pattern_for_latin_letter('a') | DOT6;
        case '2': return braille_pattern_for_latin_letter('b') | DOT6;
        case '3': return braille_pattern_for_latin_letter('c') | DOT6;
        case '4': return braille_pattern_for_latin_letter('d') | DOT6;
        case '5': return braille_pattern_for_latin_letter('e') | DOT6;
        case '6': return braille_pattern_for_latin_letter('f') | DOT6;
        case '7': return braille_pattern_for_latin_letter('g') | DOT6;
        case '8': return braille_pattern_for_latin_letter('h') | DOT6;
        case '9': return braille_pattern_for_latin_letter('i') | DOT6;
        case '0': return braille_pattern_for_latin_letter('j') | DOT6;
        case '.': return DOT2 | DOT5 | DOT6;
        case ',': return DOT2;
        case ';': return DOT2 | DOT3;
        case ':': return DOT2 | DOT5;
        case '!': return DOT2 | DOT3 | DOT5;
        case '?': return DOT2 | DOT3 | DOT6;
        case '-': return DOT3 | DOT6;
        case '\'': return DOT3;
        case '"': return DOT3 | DOT5 | DOT6;
        case '(': return DOT2 | DOT3 | DOT5 | DOT6;
        case ')': return DOT2 | DOT3 | DOT5 | DOT6;
        case '/': return DOT3 | DOT4;
        case '\\': return DOT1 | DOT2 | DOT5 | DOT6;
        case '+': return DOT2 | DOT3 | DOT5;
        case '=': return DOT1 | DOT2 | DOT3 | DOT4 | DOT5 | DOT6;
        case '*': return DOT1 | DOT6;
        case '&': return DOT1 | DOT2 | DOT3 | DOT4 | DOT6;
        case '%': return DOT1 | DOT4 | DOT6;
        case '@': return DOT4 | DOT7;
        case '#': return DOT3 | DOT4 | DOT5 | DOT6;
        case '[': return DOT1 | DOT2 | DOT6;
        case ']': return DOT3 | DOT4 | DOT5;
        case '<': return DOT1 | DOT2 | DOT6;
        case '>': return DOT3 | DOT4 | DOT5;
        case '_': return DOT4 | DOT5 | DOT6;
        default: return DOT8;
    }
}

static unsigned char braille_pattern_for_devanagari(unsigned int codepoint) {
    switch (codepoint) {
        case 0x0905:
        case 0x0906:
        case 0x093E:
            return braille_pattern_for_latin_letter('a');
        case 0x0907:
        case 0x0908:
        case 0x093F:
        case 0x0940:
            return braille_pattern_for_latin_letter('i');
        case 0x0909:
        case 0x090A:
        case 0x0941:
        case 0x0942:
            return braille_pattern_for_latin_letter('u');
        case 0x090F:
        case 0x0947:
            return braille_pattern_for_latin_letter('e');
        case 0x0910:
            return braille_pattern_for_latin_letter('e') | DOT7;
        case 0x0913:
        case 0x0914:
        case 0x094B:
        case 0x094C:
            return braille_pattern_for_latin_letter('o');
        case 0x090B:
        case 0x0943:
            return braille_pattern_for_latin_letter('r');
        case 0x0915:
        case 0x0916:
            return braille_pattern_for_latin_letter('k');
        case 0x0917:
        case 0x0918:
            return braille_pattern_for_latin_letter('g');
        case 0x091A:
        case 0x091B:
            return braille_pattern_for_latin_letter('c');
        case 0x091C:
        case 0x091D:
            return braille_pattern_for_latin_letter('j');
        case 0x091F:
        case 0x0920:
        case 0x0924:
        case 0x0925:
            return braille_pattern_for_latin_letter('t');
        case 0x0921:
        case 0x0922:
        case 0x0926:
        case 0x0927:
            return braille_pattern_for_latin_letter('d');
        case 0x0923:
        case 0x0928:
            return braille_pattern_for_latin_letter('n');
        case 0x092A:
            return braille_pattern_for_latin_letter('p');
        case 0x092B:
            return braille_pattern_for_latin_letter('f');
        case 0x092C:
        case 0x092D:
            return braille_pattern_for_latin_letter('b');
        case 0x092E:
            return braille_pattern_for_latin_letter('m');
        case 0x092F:
            return braille_pattern_for_latin_letter('y');
        case 0x0930:
            return braille_pattern_for_latin_letter('r');
        case 0x0932:
            return braille_pattern_for_latin_letter('l');
        case 0x0935:
            return braille_pattern_for_latin_letter('v');
        case 0x0936:
        case 0x0937:
        case 0x0938:
            return braille_pattern_for_latin_letter('s');
        case 0x0939:
            return braille_pattern_for_latin_letter('h');
        case 0x0902:
        case 0x0903:
        case 0x0901:
            return DOT3 | DOT5;
        case 0x094D:
            return DOT4;
        case 0x0964:
        case 0x0965:
            return braille_pattern_for_ascii('.');
        default:
            return 0;
    }
}

static int braille_ui_parse_cell_count(const char *value) {
    long parsed;
    char *end = NULL;

    if (!value || !value[0]) {
        return BRAILLE_DEFAULT_CELLS;
    }

    parsed = strtol(value, &end, 10);
    if (!end || *end != '\0') {
        return BRAILLE_DEFAULT_CELLS;
    }
    if (parsed < BRAILLE_MIN_CELLS) {
        return BRAILLE_MIN_CELLS;
    }
    if (parsed > BRAILLE_MAX_CELLS) {
        return BRAILLE_MAX_CELLS;
    }

    return (int)parsed;
}

static int braille_ui_parse_spacing(const char *value) {
    long parsed;
    char *end = NULL;

    if (!value || !value[0]) {
        return 2;
    }

    parsed = strtol(value, &end, 10);
    if (!end || *end != '\0') {
        return 2;
    }
    if (parsed < 1) {
        return 1;
    }
    if (parsed > 5) {
        return 5;
    }

    return (int)parsed;
}

static const char *braille_ui_normalize_size(const char *value) {
    if (!value || !value[0]) {
        return BRAILLE_SIZE_SMALL;
    }
    if (strcmp(value, BRAILLE_SIZE_MEDIUM) == 0) {
        return BRAILLE_SIZE_MEDIUM;
    }
    if (strcmp(value, BRAILLE_SIZE_LARGE) == 0) {
        return BRAILLE_SIZE_LARGE;
    }
    return BRAILLE_SIZE_SMALL;
}

static int utf8_decode_next(const char **cursor, unsigned int *codepoint) {
    const unsigned char *p = (const unsigned char *)*cursor;

    if (!p || !*p) {
        return 0;
    }

    if (p[0] < 0x80) {
        *codepoint = p[0];
        *cursor += 1;
        return 1;
    }

    if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        *codepoint = ((unsigned int)(p[0] & 0x1F) << 6) |
                     (unsigned int)(p[1] & 0x3F);
        *cursor += 2;
        return 1;
    }

    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        *codepoint = ((unsigned int)(p[0] & 0x0F) << 12) |
                     ((unsigned int)(p[1] & 0x3F) << 6) |
                     (unsigned int)(p[2] & 0x3F);
        *cursor += 3;
        return 1;
    }

    if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
        *codepoint = ((unsigned int)(p[0] & 0x07) << 18) |
                     ((unsigned int)(p[1] & 0x3F) << 12) |
                     ((unsigned int)(p[2] & 0x3F) << 6) |
                     (unsigned int)(p[3] & 0x3F);
        *cursor += 4;
        return 1;
    }

    *codepoint = '?';
    *cursor += 1;
    return 1;
}

static size_t utf8_encode_codepoint(unsigned int codepoint, char out[5]) {
    if (codepoint <= 0x7F) {
        out[0] = (char)codepoint;
        out[1] = '\0';
        return 1;
    }
    if (codepoint <= 0x7FF) {
        out[0] = (char)(0xC0 | ((codepoint >> 6) & 0x1F));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        out[2] = '\0';
        return 2;
    }
    if (codepoint <= 0xFFFF) {
        out[0] = (char)(0xE0 | ((codepoint >> 12) & 0x0F));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        out[3] = '\0';
        return 3;
    }

    out[0] = (char)(0xF0 | ((codepoint >> 18) & 0x07));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (char)(0x80 | (codepoint & 0x3F));
    out[4] = '\0';
    return 4;
}

static unsigned char braille_pattern_for_codepoint(unsigned int codepoint) {
    unsigned char pattern;

    if (codepoint == '\n' || codepoint == '\r' || codepoint == '\t') {
        return 0;
    }

    pattern = braille_pattern_for_ascii(codepoint);
    if (pattern != DOT8 || codepoint < 0x80) {
        return pattern;
    }

    pattern = braille_pattern_for_devanagari(codepoint);
    if (pattern != 0) {
        return pattern;
    }

    return DOT8;
}

int braille_ui_get_cell_count(void) {
    char *value = get_setting("braille_display_cells");
    int count = braille_ui_parse_cell_count(value);

    free(value);
    return count;
}

const char *braille_ui_get_size_mode(void) {
    char *value = get_setting("braille_display_size");
    const char *normalized = braille_ui_normalize_size(value);

    free(value);
    return normalized;
}

int braille_ui_get_character_spacing(void) {
    char *value = get_setting("braille_character_spacing");
    int spacing = braille_ui_parse_spacing(value);

    free(value);
    return spacing;
}

static const char *braille_ui_get_size_label(void) {
    const char *mode = braille_ui_get_size_mode();

    if (strcmp(mode, BRAILLE_SIZE_MEDIUM) == 0) {
        return menu_translate("braille_display_size_medium", "Medium");
    }
    if (strcmp(mode, BRAILLE_SIZE_LARGE) == 0) {
        return menu_translate("braille_display_size_large", "Large");
    }
    return menu_translate("braille_display_size_small", "Small");
}

char *braille_ui_get_selected_label(const char *menu_key) {
    char label[64];

    if (!menu_key) {
        return NULL;
    }

    if (strcmp(menu_key, "braille_display_cells") == 0) {
        snprintf(label,
                 sizeof(label),
                 menu_translate("braille_display_cells_value_format", "%d cells"),
                 braille_ui_get_cell_count());
        return strdup(label);
    }

    if (strcmp(menu_key, "braille_display_size") == 0) {
        return strdup(braille_ui_get_size_label());
    }

    if (strcmp(menu_key, "braille_character_spacing") == 0) {
        snprintf(label,
                 sizeof(label),
                 menu_translate("braille_character_spacing_value_format", "%d spaces"),
                 braille_ui_get_character_spacing());
        return strdup(label);
    }

    if (strcmp(menu_key, "braille_settings") == 0) {
        snprintf(label,
                 sizeof(label),
                 menu_translate("braille_settings_value_format", "%d cells, %s, %d spaces"),
                 braille_ui_get_cell_count(),
                 braille_ui_get_size_label(),
                 braille_ui_get_character_spacing());
        return strdup(label);
    }

    return NULL;
}

static unsigned char *braille_ui_collect_patterns(const char *text, int *out_count) {
    int max_cells = braille_ui_get_cell_count();
    int cells = 0;
    unsigned char *patterns;
    const char *cursor;

    if (!text || max_cells <= 0) {
        if (out_count) {
            *out_count = 0;
        }
        return NULL;
    }

    patterns = (unsigned char *)malloc((size_t)max_cells);
    if (!patterns) {
        if (out_count) {
            *out_count = 0;
        }
        return NULL;
    }

    cursor = text;
    while (*cursor && cells < max_cells) {
        unsigned int codepoint = 0;

        if (!utf8_decode_next(&cursor, &codepoint)) {
            break;
        }
        patterns[cells++] = braille_pattern_for_codepoint(codepoint);
    }

    if (out_count) {
        *out_count = cells;
    }

    return patterns;
}

int braille_ui_footer_line_count(void) {
    const char *mode = braille_ui_get_size_mode();

    if (strcmp(mode, BRAILLE_SIZE_SMALL) == 0) {
        return 1;
    }
    return 5;
}

static void braille_ui_print_small(const unsigned char *patterns, int count) {
    int spacing = braille_ui_get_character_spacing();
    size_t capacity = (size_t)(count > 0 ? count : 1) * (4 + (size_t)spacing) + 1;
    size_t length = 0;
    char *output = (char *)malloc(capacity);
    int i;

    if (!output) {
        printf("%s:\n", menu_translate("ui_braille_label", "Braille"));
        return;
    }

    for (i = 0; i < count; i++) {
        char encoded[5];
        size_t encoded_length = utf8_encode_codepoint(0x2800U + patterns[i], encoded);
        memcpy(output + length, encoded, encoded_length);
        length += encoded_length;
        if (i + 1 < count && spacing > 0) {
            memset(output + length, ' ', (size_t)spacing);
            length += (size_t)spacing;
        }
    }
    output[length] = '\0';

    printf("%s: %s\n", menu_translate("ui_braille_label", "Braille"), output);
    free(output);
}

static void braille_ui_print_zoomed(const unsigned char *patterns, int count, int large_mode) {
    static const unsigned char row_masks[4][2] = {
        {DOT1, DOT4},
        {DOT2, DOT5},
        {DOT3, DOT6},
        {DOT7, DOT8}
    };
    int row;
    int cell;
    int spacing = braille_ui_get_character_spacing();
    const char *intra_gap = large_mode ? " " : " ";
    const char *active_dot = "\xE2\x97\x8F";
    const char *inactive_dot = ".";

    printf("%s:\n", menu_translate("ui_braille_label", "Braille"));
    for (row = 0; row < 4; row++) {
        for (cell = 0; cell < count; cell++) {
            fputs((patterns[cell] & row_masks[row][0]) ? active_dot : inactive_dot, stdout);
            fputs(intra_gap, stdout);
            fputs((patterns[cell] & row_masks[row][1]) ? active_dot : inactive_dot, stdout);
            if (cell + 1 < count) {
                int gap = large_mode ? 2 + spacing : spacing;
                while (gap-- > 0) {
                    putchar(' ');
                }
            }
        }
        putchar('\n');
    }
}

void braille_ui_print_status_line(const char *text) {
    const char *mode = braille_ui_get_size_mode();
    int count = 0;
    unsigned char *patterns = braille_ui_collect_patterns(text, &count);

    if (!patterns || count <= 0) {
        if (strcmp(mode, BRAILLE_SIZE_SMALL) == 0) {
            printf("%s:\n", menu_translate("ui_braille_label", "Braille"));
        } else {
            printf("%s:\n\n\n\n\n", menu_translate("ui_braille_label", "Braille"));
        }
        free(patterns);
        return;
    }

    if (strcmp(mode, BRAILLE_SIZE_SMALL) == 0) {
        braille_ui_print_small(patterns, count);
    } else {
        braille_ui_print_zoomed(patterns, count, strcmp(mode, BRAILLE_SIZE_LARGE) == 0);
    }
    free(patterns);
}
