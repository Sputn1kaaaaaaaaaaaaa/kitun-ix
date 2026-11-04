#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_SCANCODE_BUFFER_SIZE 256

struct keyboard_event {
    uint8_t scancode;
    bool released;
};

void keyboard_init(void);
bool keyboard_poll(uint8_t *scancode, bool *released);
bool keyboard_read(struct keyboard_event *event);
void keyboard_reset(void);

/* Next typed character, or -1 when nothing is waiting.
 * Enter is '\n'. Backspace is '\b'. */
int keyboard_getchar(void);

#endif
