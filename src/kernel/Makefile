VERBOSE = off # Set this to see commands being run
COLOR   = on

include messages.make

CC      = gcc
PWD    := $(shell pwd)
CFLAGS  = -Wall -Wextra -m32 -Wa,--32 -nostdlib -nostdinc -ffreestanding -I$(PWD)
LDFLAGS = --warn-common -melf_i386 --strip-all

BOOTLOADER_PATH = arch/x86-pc/bootstrap

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
	  lib/c/printf.o \
	  lib/c/string.o \
	  memory/physical-memory.o \
	  arch/x86/paging.o \
	  arch/x86-pc/startup.o

KERNEL          = aragveli.elf
MULTIBOOT_IMAGE	= aragveli.iso

all: kernel cdrom

kernel: $(KERNEL)

$(KERNEL): $(OBJECTS)
	$(linking) '$< > $@'
	$(LD) $(LDFLAGS) -T arch/x86-pc/linker.ld -o $@ $^

%.o: %.c
	$(compiling) '$< > $@'
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.asm
	$(assembling) '$< > $@'
	nasm -f elf $<

cdrom:
	$(generating) '$@'
	mv $(KERNEL) $(BOOTLOADER_PATH)/iso/boot/
	grub-mkrescue -o $(MULTIBOOT_IMAGE) $(BOOTLOADER_PATH)/iso/

clean:
	$(cleaning)
	$(RM) $(OBJECTS)
	$(RM) $(BOOTLOADER_PATH)/iso/boot/$(KERNEL)
	$(RM) $(MULTIBOOT_IMAGE)
