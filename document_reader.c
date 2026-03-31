#include "document_reader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} StringList;

static int has_extension(const char *path, const char *ext) {
    size_t path_len;
    size_t ext_len;
    size_t i;

    if (!path || !ext) {
        return 0;
    }

    path_len = strlen(path);
    ext_len = strlen(ext);
    if (path_len < ext_len) {
        return 0;
    }

    path += path_len - ext_len;
    for (i = 0; i < ext_len; i++) {
        if (tolower((unsigned char)path[i]) != tolower((unsigned char)ext[i])) {
            return 0;
        }
    }

    return 1;
}

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) {
        return;
    }

    snprintf(error, error_size, "%s", message ? message : "Unknown error");
}

static char *duplicate_range(const char *start, size_t length) {
    char *copy = (char *)malloc(length + 1);
    if (!copy) {
        return NULL;
    }

    if (length > 0) {
        memcpy(copy, start, length);
    }
    copy[length] = '\0';
    return copy;
}

static char *shell_quote(const char *path) {
    size_t i;
    size_t extra = 2;
    char *quoted;
    char *out;

    for (i = 0; path && path[i]; i++) {
        extra++;
        if (path[i] == '\'') {
            extra += 3;
        }
    }

    quoted = (char *)malloc(extra + 1);
    if (!quoted) {
        return NULL;
    }

    out = quoted;
    *out++ = '\'';
    for (i = 0; path && path[i]; i++) {
        if (path[i] == '\'') {
            memcpy(out, "'\\''", 4);
            out += 4;
        } else {
            *out++ = path[i];
        }
    }
    *out++ = '\'';
    *out = '\0';

    return quoted;
}

static char *read_file_text(const char *path) {
    FILE *file;
    long size;
    size_t read_size;
    char *buffer;

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    read_size = fread(buffer, 1, (size_t)size, file);
    buffer[read_size] = '\0';
    fclose(file);

    return buffer;
}

static char *read_command_output(const char *command) {
    FILE *pipe;
    char *buffer;
    size_t capacity = 4096;
    size_t length = 0;
    char chunk[1024];

    pipe = popen(command, "r");
    if (!pipe) {
        return NULL;
    }

    buffer = (char *)malloc(capacity);
    if (!buffer) {
        pclose(pipe);
        return NULL;
    }

    while (fgets(chunk, sizeof(chunk), pipe) != NULL) {
        size_t chunk_len = strlen(chunk);
        if (length + chunk_len + 1 > capacity) {
            size_t new_capacity = capacity * 2;
            char *grown;
            while (length + chunk_len + 1 > new_capacity) {
                new_capacity *= 2;
            }
            grown = (char *)realloc(buffer, new_capacity);
            if (!grown) {
                free(buffer);
                pclose(pipe);
                return NULL;
            }
            buffer = grown;
            capacity = new_capacity;
        }
        memcpy(buffer + length, chunk, chunk_len);
        length += chunk_len;
    }

    if (pclose(pipe) == -1) {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

static int append_string(StringList *list, char *text) {
    char **grown;

    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        grown = (char **)realloc(list->items, new_capacity * sizeof(char *));
        if (!grown) {
            return 0;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }

    list->items[list->count++] = text;
    return 1;
}

static void free_string_list(StringList *list) {
    size_t i;
    for (i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
}

static void append_char(char **buffer, size_t *length, size_t *capacity, char ch) {
    char *grown;

    if (*length + 2 > *capacity) {
        size_t new_capacity = *capacity == 0 ? 1024 : *capacity * 2;
        grown = (char *)realloc(*buffer, new_capacity);
        if (!grown) {
            return;
        }
        *buffer = grown;
        *capacity = new_capacity;
    }

    (*buffer)[(*length)++] = ch;
    (*buffer)[*length] = '\0';
}

static void append_text(char **buffer, size_t *length, size_t *capacity, const char *text) {
    while (text && *text) {
        append_char(buffer, length, capacity, *text++);
    }
}

static void append_normalized_space(char **buffer, size_t *length, size_t *capacity) {
    if (*length == 0 || (*buffer)[*length - 1] == ' ' || (*buffer)[*length - 1] == '\n') {
        return;
    }
    append_char(buffer, length, capacity, ' ');
}

static int starts_with_ci(const char *text, const char *prefix) {
    size_t i;
    for (i = 0; prefix[i]; i++) {
        if (!text[i]) {
            return 0;
        }
        if (tolower((unsigned char)text[i]) != tolower((unsigned char)prefix[i])) {
            return 0;
        }
    }
    return 1;
}

static const char *decode_entity(const char *text, char *decoded) {
    if (strncmp(text, "&amp;", 5) == 0) {
        *decoded = '&';
        return text + 5;
    }
    if (strncmp(text, "&lt;", 4) == 0) {
        *decoded = '<';
        return text + 4;
    }
    if (strncmp(text, "&gt;", 4) == 0) {
        *decoded = '>';
        return text + 4;
    }
    if (strncmp(text, "&quot;", 6) == 0) {
        *decoded = '"';
        return text + 6;
    }
    if (strncmp(text, "&apos;", 6) == 0) {
        *decoded = '\'';
        return text + 6;
    }
    if (strncmp(text, "&nbsp;", 6) == 0) {
        *decoded = ' ';
        return text + 6;
    }
    return NULL;
}

static char *strip_markup_text(const char *input) {
    char *output = NULL;
    size_t length = 0;
    size_t capacity = 0;
    const char *p = input;
    int in_tag = 0;
    char decoded;

    while (p && *p) {
        if (!in_tag && *p == '<') {
            const char *tag_start = p + 1;
            while (*tag_start == '/' || isspace((unsigned char)*tag_start)) {
                tag_start++;
            }
            if (starts_with_ci(tag_start, "br") ||
                starts_with_ci(tag_start, "p") ||
                starts_with_ci(tag_start, "div") ||
                starts_with_ci(tag_start, "li") ||
                starts_with_ci(tag_start, "tr") ||
                starts_with_ci(tag_start, "h1") ||
                starts_with_ci(tag_start, "h2") ||
                starts_with_ci(tag_start, "h3") ||
                starts_with_ci(tag_start, "h4")) {
                if (length > 0 && output[length - 1] != '\n') {
                    append_char(&output, &length, &capacity, '\n');
                }
            }
            in_tag = 1;
            p++;
            continue;
        }

        if (in_tag) {
            if (*p == '>') {
                in_tag = 0;
            }
            p++;
            continue;
        }

        if (*p == '&') {
            const char *next = decode_entity(p, &decoded);
            if (next) {
                append_char(&output, &length, &capacity, decoded);
                p = next;
                continue;
            }
        }

        if (*p == '\r') {
            p++;
            continue;
        }

        if (*p == '\n') {
            if (length > 0 && output[length - 1] != '\n') {
                append_char(&output, &length, &capacity, '\n');
            }
            p++;
            continue;
        }

        if (isspace((unsigned char)*p)) {
            append_normalized_space(&output, &length, &capacity);
            p++;
            continue;
        }

        append_char(&output, &length, &capacity, *p);
        p++;
    }

    if (!output) {
        output = strdup("");
    }

    return output;
}

static char *join_text_blocks(StringList *blocks) {
    char *joined = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t i;

    for (i = 0; i < blocks->count; i++) {
        if (blocks->items[i] && blocks->items[i][0]) {
            if (length > 0 && joined[length - 1] != '\n') {
                append_char(&joined, &length, &capacity, '\n');
                append_char(&joined, &length, &capacity, '\n');
            }
            append_text(&joined, &length, &capacity, blocks->items[i]);
        }
    }

    if (!joined) {
        joined = strdup("");
    }

    return joined;
}

static char *zip_list_entries(const char *path) {
    char *quoted = shell_quote(path);
    char command[4096];
    char *output;

    if (!quoted) {
        return NULL;
    }

    snprintf(command, sizeof(command), "zipinfo -1 %s", quoted);
    output = read_command_output(command);
    free(quoted);
    return output;
}

static char *zip_read_entry(const char *path, const char *entry) {
    char *quoted_path = shell_quote(path);
    char *quoted_entry = shell_quote(entry);
    char command[8192];
    char *output;

    if (!quoted_path || !quoted_entry) {
        free(quoted_path);
        free(quoted_entry);
        return NULL;
    }

    snprintf(command, sizeof(command), "unzip -p %s %s 2>/dev/null", quoted_path, quoted_entry);
    output = read_command_output(command);
    free(quoted_path);
    free(quoted_entry);
    return output;
}

static int archive_entry_is_markup(const char *entry) {
    return has_extension(entry, ".xhtml") ||
           has_extension(entry, ".html") ||
           has_extension(entry, ".htm") ||
           has_extension(entry, ".xml") ||
           has_extension(entry, ".opf") ||
           has_extension(entry, ".ncx") ||
           has_extension(entry, ".smil");
}

static char *load_archive_markup_document(const char *path) {
    char *listing = zip_list_entries(path);
    char *line;
    StringList blocks;

    if (!listing) {
        return NULL;
    }

    memset(&blocks, 0, sizeof(blocks));
    line = strtok(listing, "\n");
    while (line) {
        if (archive_entry_is_markup(line)) {
            char *xml = zip_read_entry(path, line);
            if (xml) {
                char *text = strip_markup_text(xml);
                free(xml);
                if (!text || !append_string(&blocks, text)) {
                    free(text);
                    free(listing);
                    free_string_list(&blocks);
                    return NULL;
                }
            }
        }
        line = strtok(NULL, "\n");
    }

    free(listing);
    {
        char *joined = join_text_blocks(&blocks);
        free_string_list(&blocks);
        return joined;
    }
}

static char *extract_docx_text(const char *path) {
    char *quoted = shell_quote(path);
    char command[4096];
    char *output;

    if (!quoted) {
        return NULL;
    }

    snprintf(command, sizeof(command), "textutil -convert txt -stdout -- %s 2>/dev/null", quoted);
    output = read_command_output(command);
    free(quoted);

    if (output && output[0]) {
        return output;
    }
    free(output);

    output = zip_read_entry(path, "word/document.xml");
    if (!output) {
        return NULL;
    }

    {
        char *text = strip_markup_text(output);
        free(output);
        return text;
    }
}

static char *extract_pdf_text(const char *path) {
    char *quoted = shell_quote(path);
    char command[4096];
    char *output;

    if (!quoted) {
        return NULL;
    }

    snprintf(command, sizeof(command), "mdls -raw -name kMDItemTextContent -- %s 2>/dev/null", quoted);
    output = read_command_output(command);
    if (output && output[0] && strcmp(output, "(null)\n") != 0) {
        free(quoted);
        return output;
    }
    free(output);

    snprintf(command, sizeof(command), "strings -n 8 %s 2>/dev/null", quoted);
    output = read_command_output(command);
    free(quoted);
    return output;
}

static char *between_tags(const char *start, const char *open_tag, const char *close_tag, const char **next) {
    const char *open = strstr(start, open_tag);
    const char *close;
    if (!open) {
        return NULL;
    }
    open += strlen(open_tag);
    close = strstr(open, close_tag);
    if (!close) {
        return NULL;
    }
    if (next) {
        *next = close + strlen(close_tag);
    }
    return duplicate_range(open, (size_t)(close - open));
}

static char *extract_shared_strings(const char *xml, StringList *strings) {
    const char *p = xml;
    while ((p = strstr(p, "<si")) != NULL) {
        const char *end = strstr(p, "</si>");
        char *segment;
        char *text;
        if (!end) {
            break;
        }
        end += 5;
        segment = duplicate_range(p, (size_t)(end - p));
        if (!segment) {
            return NULL;
        }
        text = strip_markup_text(segment);
        free(segment);
        if (!text || !append_string(strings, text)) {
            free(text);
            return NULL;
        }
        p = end;
    }
    return strdup("");
}

static int xml_cell_type_is_shared(const char *cell_tag) {
    return strstr(cell_tag, "t=\"s\"") != NULL || strstr(cell_tag, "t='s'") != NULL;
}

static int xml_cell_type_is_inline(const char *cell_tag) {
    return strstr(cell_tag, "t=\"inlineStr\"") != NULL || strstr(cell_tag, "t='inlineStr'") != NULL;
}

static char *extract_xlsx_text(const char *path) {
    char *listing = zip_list_entries(path);
    char *shared_xml = NULL;
    StringList shared_strings;
    StringList blocks;
    char *line;

    if (!listing) {
        return NULL;
    }

    memset(&shared_strings, 0, sizeof(shared_strings));
    memset(&blocks, 0, sizeof(blocks));

    shared_xml = zip_read_entry(path, "xl/sharedStrings.xml");
    if (shared_xml) {
        char *ignored = extract_shared_strings(shared_xml, &shared_strings);
        free(ignored);
        free(shared_xml);
    }

    line = strtok(listing, "\n");
    while (line) {
        if (strstr(line, "xl/worksheets/") == line && has_extension(line, ".xml")) {
            char *sheet = zip_read_entry(path, line);
            char *out = NULL;
            size_t out_len = 0;
            size_t out_cap = 0;
            const char *p = sheet;

            if (!sheet) {
                line = strtok(NULL, "\n");
                continue;
            }

            while ((p = strstr(p, "<row")) != NULL) {
                const char *row_end = strstr(p, "</row>");
                const char *cell = p;
                if (!row_end) {
                    break;
                }

                while ((cell = strstr(cell, "<c")) != NULL && cell < row_end) {
                    const char *cell_tag_end = strchr(cell, '>');
                    const char *cell_end = strstr(cell, "</c>");
                    char *cell_tag;
                    if (!cell_tag_end || !cell_end || cell_end > row_end) {
                        break;
                    }

                    cell_tag = duplicate_range(cell, (size_t)(cell_tag_end - cell + 1));
                    if (cell_tag) {
                        char *value = NULL;
                        if (xml_cell_type_is_shared(cell_tag)) {
                            const char *next = NULL;
                            char *index_text = between_tags(cell, "<v>", "</v>", &next);
                            if (index_text) {
                                long idx = strtol(index_text, NULL, 10);
                                free(index_text);
                                if (idx >= 0 && (size_t)idx < shared_strings.count) {
                                    value = strdup(shared_strings.items[idx]);
                                }
                            }
                        } else if (xml_cell_type_is_inline(cell_tag)) {
                            char *inline_text = between_tags(cell, "<is>", "</is>", NULL);
                            if (inline_text) {
                                value = strip_markup_text(inline_text);
                                free(inline_text);
                            }
                        } else {
                            value = between_tags(cell, "<v>", "</v>", NULL);
                        }

                        if (value && value[0]) {
                            if (out_len > 0 && out[out_len - 1] != '\n') {
                                append_char(&out, &out_len, &out_cap, ' ');
                            }
                            append_text(&out, &out_len, &out_cap, value);
                        }
                        free(value);
                        free(cell_tag);
                    }

                    cell = cell_end + 4;
                }

                if (out_len > 0 && out[out_len - 1] != '\n') {
                    append_char(&out, &out_len, &out_cap, '\n');
                }
                p = row_end + 6;
            }

            if (out && out[0]) {
                if (!append_string(&blocks, out)) {
                    free(out);
                    free(sheet);
                    free(listing);
                    free_string_list(&shared_strings);
                    free_string_list(&blocks);
                    return NULL;
                }
            } else {
                free(out);
            }
            free(sheet);
        }
        line = strtok(NULL, "\n");
    }

    free(listing);
    free_string_list(&shared_strings);
    {
        char *joined = join_text_blocks(&blocks);
        free_string_list(&blocks);
        return joined;
    }
}

int document_is_supported_file(const char *path) {
    return has_extension(path, ".txt") ||
           has_extension(path, ".epub") ||
           has_extension(path, ".xhtml") ||
           has_extension(path, ".html") ||
           has_extension(path, ".htm") ||
           has_extension(path, ".docx") ||
           has_extension(path, ".pdf") ||
           has_extension(path, ".xlsx") ||
           has_extension(path, ".xls") ||
           has_extension(path, ".xml") ||
           has_extension(path, ".opf") ||
           has_extension(path, ".ncx") ||
           has_extension(path, ".smil") ||
           has_extension(path, ".daisy");
}

char *document_load_text(const char *path, char *error, size_t error_size) {
    char *raw;

    if (!document_is_supported_file(path)) {
        set_error(error, error_size, "Unsupported file type");
        return NULL;
    }

    if (has_extension(path, ".txt")) {
        raw = read_file_text(path);
        if (!raw) {
            set_error(error, error_size, "Failed to read text file");
        }
        return raw;
    }

    if (has_extension(path, ".html") || has_extension(path, ".htm") ||
        has_extension(path, ".xhtml") || has_extension(path, ".xml") ||
        has_extension(path, ".opf") || has_extension(path, ".ncx") ||
        has_extension(path, ".smil") || has_extension(path, ".daisy")) {
        raw = read_file_text(path);
        if (!raw) {
            set_error(error, error_size, "Failed to read markup file");
            return NULL;
        }
        {
            char *text = strip_markup_text(raw);
            free(raw);
            return text;
        }
    }

    if (has_extension(path, ".epub")) {
        raw = load_archive_markup_document(path);
        if (!raw) {
            set_error(error, error_size, "Failed to extract EPUB text");
        }
        return raw;
    }

    if (has_extension(path, ".docx")) {
        raw = extract_docx_text(path);
        if (!raw) {
            set_error(error, error_size, "Failed to extract DOCX text");
        }
        return raw;
    }

    if (has_extension(path, ".pdf")) {
        raw = extract_pdf_text(path);
        if (!raw) {
            set_error(error, error_size, "Failed to extract PDF text");
        }
        return raw;
    }

    if (has_extension(path, ".xlsx")) {
        raw = extract_xlsx_text(path);
        if (!raw) {
            set_error(error, error_size, "Failed to extract XLSX text");
        }
        return raw;
    }

    if (has_extension(path, ".xls")) {
        raw = extract_pdf_text(path);
        if (!raw) {
            set_error(error, error_size, "Failed to extract XLS text");
        }
        return raw;
    }

    set_error(error, error_size, "No loader available for this file");
    return NULL;
}
