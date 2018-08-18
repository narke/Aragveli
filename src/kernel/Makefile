VERBOSE = off # Set this to see commands being run
COLOR   = on

include messages.make

CC      = gcc
PWD    := $(shell pwd)
ACPICA_FLAGS = -I$(PWD)/arch/x86-pc/acpica/source/include -DACPI_USE_BUILTIN_STDARG
CFLAGS  = -Wall -Wextra -fgnu89-inline -m32 -Wa,--32 -nostdlib -nostdinc \
	  -ffreestanding -I$(PWD) $(ACPICA_FLAGS)
LDFLAGS = --warn-common -melf_i386 --strip-all

BOOTLOADER_PATH = arch/x86-pc/bootstrap
INITRD_DST = arch/x86-pc/bootstrap/iso
INITRD_SRC = ../../initrd

OBJECTS = $(BOOTLOADER_PATH)/multiboot.o \
	  arch/x86/gdt.o \
	  arch/x86/idt.o \
	  arch/x86/isr-stubs.o \
	  arch/x86/isr.o \
	  arch/x86/pic.o \
	  arch/x86/irq-stubs.o \
	  arch/x86/irq.o \
	  arch/x86/pit.o \
	  arch/x86-pc/vbe.o \
	  lib/c/stdio.o \
	  lib/c/string.o \
	  memory/frame.o \
	  memory/heap.o \
	  memory/pool.o \
	  lib/c/stdlib.o \
	  arch/x86/cpu-context.o \
	  arch/x86/cpu-context-switch.o \
	  process/thread.o \
	  fs/vfs.o \
	  fs/tarfs.o \
	  process/semaphore.o \
	  process/mutex.o \
	  process/sched_rr.o \
	  arch/x86-pc/startup.o

KERNEL          = aragveli.elf
MULTIBOOT_IMAGE	= aragveli.iso

all: kernel initrd cdrom

kernel: $(KERNEL)

$(KERNEL): $(OBJECTS)
	$(linking) '$^ > $@'
	$(LD) $(LDFLAGS) -T arch/x86-pc/linker.ld -o $@ $^

%.o: %.c
	$(compiling) '$< > $@'
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.asm
	$(assembling) '$< > $@'
	nasm -f elf $<

initrd:
	$(generating) '$@'
	tar cvf initrd.tar $(INITRD_SRC)
	mv initrd.tar $(INITRD_DST)

cdrom:
	$(generating) '$@'
	mv $(KERNEL) $(BOOTLOADER_PATH)/iso/boot/
	grub-mkrescue -o $(MULTIBOOT_IMAGE) $(BOOTLOADER_PATH)/iso/

run:
	qemu-system-i386 -m 8 -vga std -cdrom $(MULTIBOOT_IMAGE)

clean:
	$(cleaning)
	$(RM) $(OBJECTS)
	$(RM) $(BOOTLOADER_PATH)/iso/boot/$(KERNEL)
	$(RM) $(MULTIBOOT_IMAGE)
	$(RM) $(INITRD_DST)/initrd.tar
