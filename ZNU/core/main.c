
#include <stdint.h>
#include <stddef.h>

#include "../include/framebuffer.h"
#include "../include/limine.h"

// Tell Limine which protocol revision we support.
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

// Request framebuffer information.
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// Limine request markers.
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// Halt if something goes wrong.
static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void kernel_main(void) {
    // Check Limine protocol compatibility.
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        halt_forever();
    }

    // Check framebuffer response.
    if (framebuffer_request.response == NULL) {
        halt_forever();
    }

    struct limine_framebuffer_response* response =
        framebuffer_request.response;

    if (response->framebuffer_count < 1) {
        halt_forever();
    }

    // Get the first available framebuffer.
    struct limine_framebuffer* fb = response->framebuffers[0];

    // Our initial pixel implementation requires 32-bit pixels.
    if (fb->bpp != 32 ||
        fb->red_mask_size == 0 ||
        fb->red_mask_size > 8 ||
        fb->red_mask_shift + fb->red_mask_size > 32) {
        halt_forever();
    }

    // Initialize ZNU's framebuffer.
    framebuffer_init(
        (uint64_t)(uintptr_t)fb->address,
        fb->width,
        fb->height,
        fb->pitch
    );

    // Calculate the correct red pixel value.
    uint32_t red =
        ((1u << fb->red_mask_size) - 1u) << fb->red_mask_shift;

    // Draw a 50x50 red square.
    for (int y = 100; y < 150; y++) {
        for (int x = 100; x < 150; x++) {
            put_pixel(x, y, red);
        }
    }

    // Keep the kernel running.
    halt_forever();
}
