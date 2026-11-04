#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

void framebuffer_init(uint64_t address, uint64_t width,
                      uint64_t height, uint64_t pitch);

void put_pixel(int x, int y, uint32_t color);

#endif