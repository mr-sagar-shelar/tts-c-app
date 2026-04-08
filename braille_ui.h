#ifndef BRAILLE_UI_H
#define BRAILLE_UI_H

char *braille_ui_get_selected_label(void);
int braille_ui_get_cell_count(void);
char *braille_ui_build_display_text(const char *text);
void braille_ui_print_status_line(const char *text);

#endif /* BRAILLE_UI_H */
