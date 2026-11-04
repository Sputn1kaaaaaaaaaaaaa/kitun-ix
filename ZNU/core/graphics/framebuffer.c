#include <stdarg.h>
#include <stddef.h>

#include "../../include/framebuffer.h"

#define FONT_WIDTH 5
#define FONT_HEIGHT 7
#define FONT_GLYPH_SPACING 1
#define CONSOLE_MARGIN_X 16
#define CONSOLE_MARGIN_Y 16

static volatile uint32_t* framebuffer;
static uint64_t screen_width;
static uint64_t screen_height;
static uint64_t pitch_pixels;

static uint32_t console_fg = 0xFFFFFFFFu;
static uint32_t console_bg = 0x00000000u;
static int console_x = 0;
static int console_y = 0;

static int console_columns(void) {
    if (screen_width <= CONSOLE_MARGIN_X * 2) {
        return 1;
    }
    return (int)((screen_width - CONSOLE_MARGIN_X * 2) / (FONT_WIDTH + FONT_GLYPH_SPACING));
}

static int console_rows(void) {
    if (screen_height <= CONSOLE_MARGIN_Y * 2) {
        return 1;
    }
    return (int)((screen_height - CONSOLE_MARGIN_Y * 2) / (FONT_HEIGHT + FONT_GLYPH_SPACING));
}

static void console_scroll(void) {
    int rows = console_rows();
    int cols = console_columns();
    if (rows <= 1 || cols <= 0) {
        console_x = 0;
        console_y = 0;
        return;
    }

    for (int y = 0; y < rows - 1; y++) {
        for (int x = 0; x < cols; x++) {
            int src_x = CONSOLE_MARGIN_X + x * (FONT_WIDTH + FONT_GLYPH_SPACING);
            int src_y = CONSOLE_MARGIN_Y + y * (FONT_HEIGHT + FONT_GLYPH_SPACING);

            for (int row = 0; row < FONT_HEIGHT; row++) {
                for (int col = 0; col < FONT_WIDTH; col++) {
                    int px = src_x + col;
                    int py = src_y + row;

                    if ((uint64_t)px < screen_width && (uint64_t)py < screen_height) {
                        uint32_t color = framebuffer[py * pitch_pixels + px];
                        int dst_x = px;
                        int dst_y = py + (FONT_HEIGHT + FONT_GLYPH_SPACING);
                        if ((uint64_t)dst_x < screen_width && (uint64_t)dst_y < screen_height) {
                            framebuffer[dst_y * pitch_pixels + dst_x] = color;
                        }
                    }
                }
            }
        }
    }

    console_x = 0;
    console_y = rows - 1;
}

static void console_clear_area(int x, int y, int width, int height) {
    if (!framebuffer) return;

    for (int yy = 0; yy < height; yy++) {
        for (int xx = 0; xx < width; xx++) {
            int px = x + xx;
            int py = y + yy;
            if ((uint64_t)px < screen_width && (uint64_t)py < screen_height) {
                framebuffer[py * pitch_pixels + px] = console_bg;
            }
        }
    }
}

static const uint8_t font_bitmap[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, /* space */
    0x00, 0x00, 0x5F, 0x00, 0x00, /* ! */
    0x00, 0x07, 0x00, 0x07, 0x00, /* " */
    0x14, 0x7F, 0x14, 0x7F, 0x14, /* # */
    0x24, 0x2A, 0x7F, 0x2A, 0x12, /* $ */
    0x23, 0x13, 0x08, 0x64, 0x62, /* % */
    0x36, 0x49, 0x55, 0x22, 0x50, /* & */
    0x00, 0x05, 0x03, 0x00, 0x00, /* ' */
    0x00, 0x1C, 0x22, 0x41, 0x00, /* ( */
    0x00, 0x41, 0x22, 0x1C, 0x00, /* ) */
    0x08, 0x2A, 0x1C, 0x2A, 0x08, /* * */
    0x08, 0x08, 0x3E, 0x08, 0x08, /* + */
    0x00, 0x50, 0x30, 0x00, 0x00, /* , */
    0x08, 0x08, 0x08, 0x08, 0x08, /* - */
    0x00, 0x60, 0x60, 0x00, 0x00, /* . */
    0x20, 0x10, 0x08, 0x04, 0x02, /* / */
    0x3E, 0x51, 0x49, 0x45, 0x3E, /* 0 */
    0x00, 0x42, 0x7F, 0x40, 0x00, /* 1 */
    0x42, 0x61, 0x51, 0x49, 0x46, /* 2 */
    0x21, 0x41, 0x45, 0x4B, 0x31, /* 3 */
    0x18, 0x14, 0x12, 0x7F, 0x10, /* 4 */
    0x27, 0x45, 0x45, 0x45, 0x39, /* 5 */
    0x3C, 0x4A, 0x49, 0x49, 0x30, /* 6 */
    0x01, 0x71, 0x09, 0x05, 0x03, /* 7 */
    0x36, 0x49, 0x49, 0x49, 0x36, /* 8 */
    0x06, 0x49, 0x49, 0x29, 0x1E, /* 9 */
    0x00, 0x36, 0x36, 0x00, 0x00, /* : */
    0x00, 0x56, 0x36, 0x00, 0x00, /* ; */
    0x00, 0x08, 0x14, 0x22, 0x41, /* < */
    0x14, 0x14, 0x14, 0x14, 0x14, /* = */
    0x41, 0x22, 0x14, 0x08, 0x00, /* > */
    0x02, 0x01, 0x51, 0x09, 0x06, /* ? */
    0x32, 0x49, 0x79, 0x41, 0x3E, /* @ */
    0x7E, 0x11, 0x11, 0x11, 0x7E, /* A */
    0x7F, 0x49, 0x49, 0x49, 0x36, /* B */
    0x3E, 0x41, 0x41, 0x41, 0x22, /* C */
    0x7F, 0x41, 0x41, 0x22, 0x1C, /* D */
    0x7F, 0x49, 0x49, 0x49, 0x41, /* E */
    0x7F, 0x09, 0x09, 0x09, 0x01, /* F */
    0x3E, 0x41, 0x49, 0x49, 0x7A, /* G */
    0x7F, 0x08, 0x08, 0x08, 0x7F, /* H */
    0x00, 0x41, 0x7F, 0x41, 0x00, /* I */
    0x20, 0x40, 0x41, 0x3F, 0x01, /* J */
    0x7F, 0x08, 0x14, 0x22, 0x41, /* K */
    0x7F, 0x40, 0x40, 0x40, 0x40, /* L */
    0x7F, 0x02, 0x04, 0x02, 0x7F, /* M */
    0x7F, 0x04, 0x08, 0x10, 0x7F, /* N */
    0x3E, 0x41, 0x41, 0x41, 0x3E, /* O */
    0x7F, 0x09, 0x09, 0x09, 0x06, /* P */
    0x3E, 0x41, 0x51, 0x21, 0x5E, /* Q */
    0x7F, 0x09, 0x19, 0x29, 0x46, /* R */
    0x46, 0x49, 0x49, 0x49, 0x31, /* S */
    0x01, 0x01, 0x7F, 0x01, 0x01, /* T */
    0x3F, 0x40, 0x40, 0x40, 0x3F, /* U */
    0x1F, 0x20, 0x40, 0x20, 0x1F, /* V */
    0x7F, 0x20, 0x18, 0x20, 0x7F, /* W */
    0x63, 0x14, 0x08, 0x14, 0x63, /* X */
    0x03, 0x04, 0x78, 0x04, 0x03, /* Y */
    0x61, 0x51, 0x49, 0x45, 0x43, /* Z */
    0x00, 0x7F, 0x41, 0x41, 0x00, /* [ */
    0x02, 0x04, 0x08, 0x10, 0x20, /* \ */
    0x00, 0x41, 0x41, 0x7F, 0x00, /* ] */
    0x04, 0x02, 0x01, 0x02, 0x04, /* ^ */
    0x40, 0x40, 0x40, 0x40, 0x40, /* _ */
    0x00, 0x01, 0x02, 0x04, 0x00, /* ` */
    0x20, 0x54, 0x54, 0x54, 0x78, /* a */
    0x7F, 0x48, 0x44, 0x44, 0x38, /* b */
    0x38, 0x44, 0x44, 0x44, 0x20, /* c */
    0x38, 0x44, 0x44, 0x48, 0x7F, /* d */
    0x38, 0x54, 0x54, 0x54, 0x18, /* e */
    0x08, 0x7E, 0x09, 0x01, 0x02, /* f */
    0x18, 0xA4, 0xA4, 0xA4, 0x7C, /* g */
    0x7F, 0x08, 0x04, 0x04, 0x78, /* h */
    0x00, 0x44, 0x7D, 0x40, 0x00, /* i */
    0x20, 0x40, 0x44, 0x3D, 0x00, /* j */
    0x7F, 0x10, 0x28, 0x44, 0x00, /* k */
    0x00, 0x41, 0x7F, 0x40, 0x00, /* l */
    0x7C, 0x04, 0x18, 0x04, 0x78, /* m */
    0x7C, 0x08, 0x04, 0x04, 0x78, /* n */
    0x38, 0x44, 0x44, 0x44, 0x38, /* o */
    0xFC, 0x24, 0x24, 0x24, 0x18, /* p */
    0x18, 0x24, 0x24, 0x24, 0xFC, /* q */
    0x00, 0x7C, 0x08, 0x04, 0x04, /* r */
    0x48, 0x54, 0x54, 0x54, 0x20, /* s */
    0x04, 0x3F, 0x44, 0x40, 0x20, /* t */
    0x3C, 0x40, 0x40, 0x20, 0x7C, /* u */
    0x1C, 0x20, 0x40, 0x20, 0x1C, /* v */
    0x3C, 0x40, 0x38, 0x40, 0x3C, /* w */
    0x44, 0x28, 0x10, 0x28, 0x44, /* x */
    0x1C, 0xA0, 0xA0, 0xA0, 0x7C, /* y */
    0x44, 0x64, 0x54, 0x4C, 0x44, /* z */
    0x00, 0x08, 0x36, 0x41, 0x00, /* { */
    0x00, 0x00, 0x7F, 0x00, 0x00, /* | */
    0x00, 0x41, 0x36, 0x08, 0x00, /* } */
    0x02, 0x01, 0x02, 0x04, 0x02 /* ~ */
};

static void draw_glyph(int x, int y, char c, uint32_t color) {
    if (x < 0 || y < 0 || !framebuffer) return;

    int idx = (int)(unsigned char)c - 32;
    if (idx < 0 || idx > 94) {
        idx = 0;
    }

    /* Each glyph is 5 column bytes. Bit 0 is the top pixel. */
    for (int col = 0; col < FONT_WIDTH; col++) {
        uint8_t bits = font_bitmap[idx * FONT_WIDTH + col];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            if ((bits >> row) & 1u) {
                int px = x + col;
                int py = y + row;
                if ((uint64_t)px < screen_width && (uint64_t)py < screen_height) {
                    framebuffer[py * pitch_pixels + px] = color;
                }
            }
        }
    }
}

void framebuffer_init(uint64_t address, uint64_t width,
                      uint64_t height, uint64_t pitch) {
    framebuffer = (volatile uint32_t*)address;
    screen_width = width;
    screen_height = height;
    pitch_pixels = pitch / 4;
    console_x = 0;
    console_y = 0;
    console_fg = 0xFFFFFFFFu;
    console_bg = 0x00000000u;
    console_clear();
}

void put_pixel(int x, int y, uint32_t color) {
    if (!framebuffer) return;

    if (x < 0 || y < 0) return;
    if ((uint64_t)x >= screen_width) return;
    if ((uint64_t)y >= screen_height) return;

    framebuffer[y * pitch_pixels + x] = color;
}

void console_init(void) {
    console_x = 0;
    console_y = 0;
    console_fg = 0xFFFFFFFFu;
    console_bg = 0x00000000u;
    console_clear();
}

void console_set_colors(uint32_t fg, uint32_t bg) {
    console_fg = fg;
    console_bg = bg;
}

void console_clear(void) {
    int columns = console_columns();
    int rows = console_rows();
    if (columns <= 0 || rows <= 0) {
        return;
    }

    console_clear_area(CONSOLE_MARGIN_X, CONSOLE_MARGIN_Y,
                       (screen_width - CONSOLE_MARGIN_X * 2),
                       (screen_height - CONSOLE_MARGIN_Y * 2));

    console_x = 0;
    console_y = 0;
}

static void console_newline(void) {
    int cols = console_columns();
    int rows = console_rows();

    if (cols <= 0 || rows <= 0) {
        console_x = 0;
        console_y = 0;
        return;
    }

    console_x = 0;
    console_y++;
    if (console_y >= rows) {
        console_y = rows - 1;
        console_scroll();
    }
}

void console_putc(char c) {
    if (!framebuffer) return;

    if (c == '\n') {
        console_newline();
        return;
    }

    if (c == '\r') {
        console_x = 0;
        return;
    }

    if (c == '\t') {
        console_x += 4;
        if (console_x >= console_columns()) {
            console_newline();
        }
        return;
    }

    if (c == '\b') {
        if (console_x <= 0) {
            return;
        }
        console_x--;
        int glyph_x = CONSOLE_MARGIN_X + console_x * (FONT_WIDTH + FONT_GLYPH_SPACING);
        int glyph_y = CONSOLE_MARGIN_Y + console_y * (FONT_HEIGHT + FONT_GLYPH_SPACING);
        console_clear_area(glyph_x, glyph_y, FONT_WIDTH, FONT_HEIGHT);
        return;
    }

    int cols = console_columns();
    int rows = console_rows();
    if (cols <= 0 || rows <= 0) {
        return;
    }

    if (console_x >= cols) {
        console_newline();
    }

    int glyph_x = CONSOLE_MARGIN_X + console_x * (FONT_WIDTH + FONT_GLYPH_SPACING);
    int glyph_y = CONSOLE_MARGIN_Y + console_y * (FONT_HEIGHT + FONT_GLYPH_SPACING);

    if ((uint64_t)(glyph_x + FONT_WIDTH) < screen_width &&
        (uint64_t)(glyph_y + FONT_HEIGHT) < screen_height) {
        console_clear_area(glyph_x, glyph_y, FONT_WIDTH, FONT_HEIGHT);
        draw_glyph(glyph_x, glyph_y, c, console_fg);
    }

    console_x++;
}

void console_puts(const char *s) {
    if (!s) return;
    for (const char *p = s; *p != '\0'; p++) {
        console_putc(*p);
    }
}

static void console_write_unsigned(uint64_t value, int base, int uppercase) {
    char digits[] = "0123456789abcdef";
    if (uppercase) {
        digits[10] = 'A';
        digits[11] = 'B';
        digits[12] = 'C';
        digits[13] = 'D';
        digits[14] = 'E';
        digits[15] = 'F';
    }

    char buffer[32];
    int idx = 0;

    do {
        buffer[idx++] = digits[value % (uint64_t)base];
        value /= (uint64_t)base;
    } while (value != 0 && idx < 31);

    while (idx-- > 0) {
        console_putc(buffer[idx]);
    }
}

static void console_write_signed(int64_t value) {
    if (value < 0) {
        console_putc('-');
        value = -value;
    }
    console_write_unsigned((uint64_t)value, 10, 0);
}

int printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int written = 0;
    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            console_putc(*p);
            written++;
            continue;
        }

        p++;
        if (*p == '\0') {
            break;
        }

        switch (*p) {
            case 'c': {
                char ch = (char)va_arg(args, int);
                console_putc(ch);
                written++;
                break;
            }
            case 'd':
            case 'i': {
                int value = va_arg(args, int);
                console_write_signed((int64_t)value);
                written += 1;
                break;
            }
            case 'u': {
                unsigned int value = va_arg(args, unsigned int);
                console_write_unsigned((uint64_t)value, 10, 0);
                written += 1;
                break;
            }
            case 'x': {
                unsigned int value = va_arg(args, unsigned int);
                console_write_unsigned((uint64_t)value, 16, 0);
                written += 1;
                break;
            }
            case 'X': {
                unsigned int value = va_arg(args, unsigned int);
                console_write_unsigned((uint64_t)value, 16, 1);
                written += 1;
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (s == NULL) {
                    s = "(null)";
                }
                for (const char *q = s; *q != '\0'; q++) {
                    console_putc(*q);
                    written++;
                }
                break;
            }
            case '%':
                console_putc('%');
                written++;
                break;
            default:
                console_putc('%');
                console_putc(*p);
                written += 2;
                break;
        }
    }

    va_end(args);
    return written;
}

