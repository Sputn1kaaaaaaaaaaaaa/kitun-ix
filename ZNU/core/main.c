
#include <stdint.h>
#include <stddef.h>

#include "../include/framebuffer.h"
#include "../include/limine.h"
#ifdef CONFIG_DRIVERS_KEYBOARD
#include "keyboard.h"
#endif

// tell Limine which protocol revision we support
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

// request framebuffer information
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

// limine request markers
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

// halt if something goes wrong
static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void kernel_main(void) {
    // check Limine protocol compatibility
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        halt_forever();
    }

    // check framebuffer response
    if (framebuffer_request.response == NULL) {
        halt_forever();
    }

    struct limine_framebuffer_response* response =
        framebuffer_request.response;

    if (response->framebuffer_count < 1) {
        halt_forever();
    }

    // get the first available framebuffer
    struct limine_framebuffer* fb = response->framebuffers[0];

    // our initial pixel implementation requires 32-bit pixels
    if (fb->bpp != 32 ||
        fb->red_mask_size == 0 ||
        fb->red_mask_size > 8 ||
        fb->red_mask_shift + fb->red_mask_size > 32) {
        halt_forever();
    }

    // initialize ZNU's framebuffer and console
    framebuffer_init(
        (uint64_t)(uintptr_t)fb->address,
        fb->width,
        fb->height,
        fb->pitch
    );
    console_init();
    console_set_colors(0xFFFFFFFFu, 0x00000000u);
    printk("ZNU booted\n");
    printk("framebuffer: %ux%u\n", fb->width, fb->height);
    printk("hello from printk()!\n");

#ifdef CONFIG_DRIVERS_KEYBOARD
    keyboard_init();

    for (;;) {
        int ch = keyboard_getchar();
        if (ch < 0) {
            __asm__ volatile ("pause");
            continue;
        }
        console_putc((char)ch);
    }
#else
    printk("keyboard driver disabled\n");
    halt_forever();
#endif
}
