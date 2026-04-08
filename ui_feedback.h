#ifndef UI_FEEDBACK_H
#define UI_FEEDBACK_H

typedef enum {
    UI_FEEDBACK_ERROR = 0,
    UI_FEEDBACK_WARNING,
    UI_FEEDBACK_CONFIRMATION
} UIFeedbackSound;

void ui_feedback_init(void);
void ui_feedback_shutdown(void);
void ui_feedback_play(UIFeedbackSound sound);

#endif /* UI_FEEDBACK_H */
