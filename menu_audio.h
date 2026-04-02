#ifndef MENU_AUDIO_H
#define MENU_AUDIO_H

void menu_audio_init(void);
void menu_audio_shutdown(void);
void menu_audio_request(const char *text);
void menu_audio_speak(const char *text);
void menu_audio_stop(void);
int menu_audio_is_enabled(void);

#endif /* MENU_AUDIO_H */
