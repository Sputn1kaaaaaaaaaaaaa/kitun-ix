CC = gcc
LD = ld
QEMU ?= qemu-system-x86_64
LIMINE_DIR ?= /usr/share/limine

CFLAGS = -m64 -mcmodel=kernel -ffreestanding -fno-stack-protector -fno-pic -mno-red-zone -Wall -Wextra -Werror -Iinclude -Idrivers/keyboard
LDFLAGS = -m elf_x86_64 -nostdlib -static -n -z noexecstack -T linker.ld

KERNEL = kernel.elf
ISO = znu.iso
ISO_ROOT = iso_root

# menuconfig writes .config. A missing file keeps the previous default: on.
ifneq ($(wildcard .config),)
include .config
else
CONFIG_DRIVERS := y
CONFIG_DRIVERS_KEYBOARD := y
endif

OBJS = boot/boot.o core/main.o core/graphics/framebuffer.o
# The keyboard driver runs only when both its menu and the drivers menu are on.
ifeq ($(CONFIG_DRIVERS),y)
ifeq ($(CONFIG_DRIVERS_KEYBOARD),y)
OBJS += drivers/keyboard/keyboard.o
CFLAGS += -DCONFIG_DRIVERS_KEYBOARD
endif
endif

.PHONY: all clean iso run menuconfig

all: iso

iso: $(KERNEL)
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/boot $(ISO_ROOT)/EFI/BOOT
	cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
	cp boot/limine.conf $(ISO_ROOT)/boot/limine.conf
	cp boot/limine.conf $(ISO_ROOT)/boot/limine.cfg
	cp boot/limine.conf $(ISO_ROOT)/limine.conf
	cp boot/limine.conf $(ISO_ROOT)/limine.cfg
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/boot/limine-bios.sys
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine-bios-cd.bin
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine-uefi-cd.bin
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI
	xorriso -as mkisofs -R -r -J \
		-b boot/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_ROOT) -o $(ISO)
	limine bios-install $(ISO)

run: iso
	$(QEMU) -m 2G -cdrom $(ISO) -vga std -display gtk

menuconfig:
	python3 scripts/menuconfig.py

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

boot/boot.o: boot/boot.asm
	$(CC) -x assembler-with-cpp -c $< -o $@

core/main.o: core/main.c include/framebuffer.h include/limine.h drivers/keyboard/keyboard.h $(wildcard .config)
	$(CC) $(CFLAGS) -c $< -o $@

core/graphics/framebuffer.o: core/graphics/framebuffer.c include/framebuffer.h
	$(CC) $(CFLAGS) -c $< -o $@

drivers/keyboard/keyboard.o: drivers/keyboard/keyboard.c drivers/keyboard/keyboard.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(KERNEL) $(ISO)
	rm -rf $(ISO_ROOT)
