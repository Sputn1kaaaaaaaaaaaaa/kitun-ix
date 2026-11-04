#include <stddef.h>

#include "keyboard.h"

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_CMD 0x64

static volatile uint8_t keyboard_buffer[KEYBOARD_SCANCODE_BUFFER_SIZE];
static volatile size_t keyboard_head = 0;
static volatile size_t keyboard_tail = 0;
static bool shift_down = false;
static bool caps_lock = false;
static bool extended = false;

/* Set 1 scancodes, which is what the controller produces with translation on. */
static const char unshifted[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/',
    [0x37] = '*',
    [0x39] = ' ',
};

static const char shifted[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\n',
    [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2B] = '|',
    [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*',
    [0x39] = ' ',
};

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static bool ps2_wait_write(void) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x02) == 0) {
            return true;
        }
    }
    return false;
}

static bool ps2_wait_read(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) {
            return true;
        }
    }
    return false;
}

static void ps2_flush(void) {
    for (int i = 0; i < 16; i++) {
        if ((inb(PS2_STATUS) & 0x01) == 0) {
            return;
        }
        (void)inb(PS2_DATA);
    }
}

static inline void keyboard_enqueue(uint8_t scancode) {
    size_t next = (keyboard_head + 1u) % KEYBOARD_SCANCODE_BUFFER_SIZE;
    if (next != keyboard_tail) {
        keyboard_buffer[keyboard_head] = scancode;
        keyboard_head = next;
    }
}

static inline bool keyboard_dequeue(uint8_t *scancode) {
    if (keyboard_head == keyboard_tail) {
        return false;
    }

    *scancode = keyboard_buffer[keyboard_tail];
    keyboard_tail = (keyboard_tail + 1u) % KEYBOARD_SCANCODE_BUFFER_SIZE;
    return true;
}

static void keyboard_drain(void) {
    for (int i = 0; i < 16; i++) {
        uint8_t status = inb(PS2_STATUS);
        if ((status & 0x01) == 0) {
            return;
        }
        uint8_t data = inb(PS2_DATA);
        if (status & 0x20) {
            continue;
        }
        keyboard_enqueue(data);
    }
}

void keyboard_init(void) {
    keyboard_head = 0;
    keyboard_tail = 0;
    shift_down = false;
    caps_lock = false;
    extended = false;

    if (!ps2_wait_write()) {
        return;
    }
    outb(PS2_CMD, 0xAD);
    if (!ps2_wait_write()) {
        return;
    }
    outb(PS2_CMD, 0xA7);
    ps2_flush();

    if (!ps2_wait_write()) {
        return;
    }
    outb(PS2_CMD, 0x20);
    if (!ps2_wait_read()) {
        return;
    }

    uint8_t config = inb(PS2_DATA);
    config |= 0x40;
    config &= (uint8_t)~0x03;
    config &= (uint8_t)~0x10;
    config |= 0x20;

    if (!ps2_wait_write()) {
        return;
    }
    outb(PS2_CMD, 0x60);
    if (!ps2_wait_write()) {
        return;
    }
    outb(PS2_DATA, config);

    if (!ps2_wait_write()) {
        return;
    }
    outb(PS2_CMD, 0xAE);

    if (!ps2_wait_write()) {
        return;
    }
    outb(PS2_DATA, 0xF4);
    if (ps2_wait_read()) {
        (void)inb(PS2_DATA);
    }
    ps2_flush();
}

bool keyboard_poll(uint8_t *scancode, bool *released) {
    if (scancode == NULL || released == NULL) {
        return false;
    }

    keyboard_drain();

    uint8_t value = 0;
    if (!keyboard_dequeue(&value)) {
        return false;
    }

    *scancode = (uint8_t)(value & 0x7Fu);
    *released = (value & 0x80u) != 0;
    return true;
}

bool keyboard_read(struct keyboard_event *event) {
    if (event == NULL) {
        return false;
    }

    keyboard_drain();

    uint8_t value = 0;
    if (!keyboard_dequeue(&value)) {
        return false;
    }

    event->scancode = (uint8_t)(value & 0x7Fu);
    event->released = (value & 0x80u) != 0;
    return true;
}

void keyboard_reset(void) {
    keyboard_init();
}

void keyboard_handle_scancode(uint8_t scancode) {
    keyboard_enqueue(scancode);
}

static char decode_make(uint8_t code) {
    char ch = unshifted[code];
    if (ch == 0) {
        return 0;
    }
    if (ch >= 'a' && ch <= 'z') {
        if (shift_down ^ caps_lock) {
            ch = (char)(ch - 'a' + 'A');
        }
        return ch;
    }
    if (shift_down && shifted[code] != 0) {
        return shifted[code];
    }
    return ch;
}

int keyboard_getchar(void) {
    keyboard_drain();

    while (keyboard_head != keyboard_tail) {
        uint8_t raw = 0;
        if (!keyboard_dequeue(&raw)) {
            break;
        }

        if (raw == 0xE0) {
            extended = true;
            continue;
        }

        bool released = (raw & 0x80u) != 0;
        uint8_t code = (uint8_t)(raw & 0x7Fu);

        if (extended) {
            extended = false;
            if (!released && code == 0x1C) {
                return '\n';
            }
            continue;
        }

        if (code == 0x2A || code == 0x36) {
            shift_down = !released;
            continue;
        }

        if (released) {
            continue;
        }

        if (code == 0x3A) {
            caps_lock = !caps_lock;
            continue;
        }

        char ch = decode_make(code);
        if (ch != 0) {
            return (unsigned char)ch;
        }
    }

    return -1;
}
