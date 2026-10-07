VERBOSE = off # Set this to see commands being run
COLOR   = on

include messages.make

CC      = gcc
PWD    := $(shell pwd)
CFLAGS  = -Wall -Wextra -Wconversion -Wsign-conversion -Wformat-security \
	  -m32 -Wa,--32 -nostdlib -nostdinc \
	  -ffreestanding -Wimplicit-fallthrough=0 -I$(PWD)
LDFLAGS = --warn-common -melf_i386 --strip-all

BOOTLOADER_PATH = arch/x86-pc/bootstrap
INITRD_DST = arch/x86-pc/bootstrap/iso
INITRD_PATH = ..

ASM_SOURCES=$(shell find . -name "*.asm")
C_SOURCES=$(shell find . -name "*.c")
OBJECTS=$(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)

KERNEL          = aragveli.elf
MULTIBOOT_IMAGE	= aragveli.iso

.PHONY: all kernel initrd cdrom run clean

all: cdrom

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
	tar cvf initrd.tar -C $(INITRD_PATH) initrd/
	mv initrd.tar $(INITRD_DST)

cdrom: kernel initrd
	$(generating) '$@'
	mv $(KERNEL) $(BOOTLOADER_PATH)/iso/boot/
	grub-mkrescue -o $(MULTIBOOT_IMAGE) $(BOOTLOADER_PATH)/iso/

run:
	qemu-system-i386 -m 256 -vga std \
		-netdev user,id=net0 \
		-device rtl8139,netdev=net0 \
		-smp cores=4,threads=2 \
		-monitor stdio \
		-cdrom $(MULTIBOOT_IMAGE)

clean:
	$(cleaning)
	$(RM) $(OBJECTS)
	$(RM) $(BOOTLOADER_PATH)/iso/boot/$(KERNEL)
	$(RM) $(MULTIBOOT_IMAGE)
	$(RM) $(INITRD_DST)/initrd.tar
