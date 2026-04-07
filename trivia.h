#ifndef TRIVIA_H
#define TRIVIA_H

void init_trivia(void);
void cleanup_trivia(void);
char *trivia_get_selected_label(const char *menu_key);
void trivia_show_settings_menu(const char *menu_key);
void trivia_run_quiz(void);

#endif /* TRIVIA_H */
