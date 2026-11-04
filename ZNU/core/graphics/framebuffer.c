#include "../../include/framebuffer.h"

static volatile uint32_t* framebuffer;
static uint64_t screen_width;
static uint64_t screen_height;
static uint64_t pitch_pixels;

void framebuffer_init(uint64_t address, uint64_t width,
                      uint64_t height, uint64_t pitch) {
    framebuffer = (volatile uint32_t*)address;
    screen_width = width;
    screen_height = height;
    pitch_pixels = pitch / 4;
}

void put_pixel(int x, int y, uint32_t color) {
    if (!framebuffer) return;

    if (x < 0 || y < 0) return;
    if ((uint64_t)x >= screen_width) return;
    if ((uint64_t)y >= screen_height) return;

    framebuffer[y * pitch_pixels + x] = color;
}