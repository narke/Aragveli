VERBOSE = off # Set this to see commands being run
COLOR   = on

include messages.make

CC      = gcc
PWD    := $(shell pwd)
CFLAGS  = -Wall -Wextra -Wconversion -Wsign-conversion -fgnu89-inline -m32 -Wa,--32 -nostdlib -nostdinc \
	  -ffreestanding -Wimplicit-fallthrough=0 -I$(PWD) -I../extra
LDFLAGS = --warn-common -melf_i386 --strip-all

BOOTLOADER_PATH = arch/x86-pc/bootstrap
INITRD_DST = arch/x86-pc/bootstrap/iso
INITRD_PATH = ../extra/

OBJECTS = $(BOOTLOADER_PATH)/multiboot.o \
	  arch/x86/gdt.o \
	  arch/x86/idt.o \
	  arch/x86/isr-stubs.o \
	  arch/x86/isr.o \
	  arch/x86/pic.o \
	  arch/x86/irq-stubs.o \
	  arch/x86/irq.o \
	  arch/x86/pit.o \
	  arch/x86/acpi.o \
	  arch/x86/ioapic.o \
	  arch/x86/lapic.o \
	  arch/x86/smp.o \
	  lib/c/stdio.o \
	  lib/c/string.o \
	  memory/frame.o \
	  memory/heap.o \
	  memory/pool.o \
	  lib/c/stdlib.o \
	  arch/x86/cpu-context.o \
	  arch/x86/cpu-context-switch.o \
	  process/thread.o \
	  process/semaphore.o \
	  process/mutex.o \
	  process/sched_rr.o \
	  ../extra/drivers/vbe.o \
	  ../extra/drivers/pci.o \
	  ../extra/drivers/rtl8139.o \
	  ../extra/fs/vfs.o \
	  ../extra/fs/tarfs.o \
	  ../extra/test-suite/tarfs-test.o \
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
	tar cvf initrd.tar -C $(INITRD_PATH) initrd/
	mv initrd.tar $(INITRD_DST)

cdrom:
	$(generating) '$@'
	mv $(KERNEL) $(BOOTLOADER_PATH)/iso/boot/
	grub-mkrescue -o $(MULTIBOOT_IMAGE) $(BOOTLOADER_PATH)/iso/

run:
	qemu-system-i386 -m 8 -vga std \
		-netdev user,id=net0 \
		-device rtl8139,netdev=net0 \
		-smp cores=4,threads=2 \
		-cdrom $(MULTIBOOT_IMAGE)

clean:
	$(cleaning)
	$(RM) $(OBJECTS)
	$(RM) $(BOOTLOADER_PATH)/iso/boot/$(KERNEL)
	$(RM) $(MULTIBOOT_IMAGE)
	$(RM) $(INITRD_DST)/initrd.tar
