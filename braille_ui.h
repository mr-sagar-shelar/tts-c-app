#ifndef BRAILLE_UI_H
#define BRAILLE_UI_H

char *braille_ui_get_selected_label(const char *menu_key);
int braille_ui_get_cell_count(void);
const char *braille_ui_get_size_mode(void);
int braille_ui_get_character_spacing(void);
int braille_ui_footer_line_count(void);
void braille_ui_print_status_line(const char *text);

#endif /* BRAILLE_UI_H */
