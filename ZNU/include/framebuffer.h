#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdarg.h>
#include <stdint.h>

void framebuffer_init(uint64_t address, uint64_t width,
                      uint64_t height, uint64_t pitch);

void put_pixel(int x, int y, uint32_t color);

void console_init(void);
void console_clear(void);
void console_set_colors(uint32_t fg, uint32_t bg);
void console_putc(char c);
void console_puts(const char *s);
int printk(const char *fmt, ...);

#endif