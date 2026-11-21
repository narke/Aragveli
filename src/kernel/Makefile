VERBOSE = off # Set this to see commands being run
COLOR   = on

include messages.make

CC      = gcc
PWD    := $(shell pwd)
LWIP_DIR      = net/lwip/src
LWIP_INCLUDES = -isystem $(PWD)/net/port/include -isystem $(PWD)/$(LWIP_DIR)/include
CFLAGS  = -Wall -Wextra -Wconversion -Wsign-conversion -Wformat-security \
	  -m32 -Wa,--32 -nostdlib -nostdinc \
	  -ffreestanding -Wimplicit-fallthrough=0 -I$(PWD) $(LWIP_INCLUDES)
LDFLAGS = --warn-common -melf_i386 --strip-all

BOOTLOADER_PATH = arch/x86-pc/bootstrap
INITRD_DST = arch/x86-pc/bootstrap/iso
INITRD_PATH = ..

FATFS_DIR      = fs/fatfs
FATFS_INCLUDES = -isystem $(PWD)/$(FATFS_DIR)/port/include
CFLAGS  += $(FATFS_INCLUDES)

ASM_SOURCES=$(shell find . -name "*.asm")
C_SOURCES=$(shell find . -name "*.c" -not -path "./net/lwip/*" \
	  -not -path "./$(FATFS_DIR)/ff.c" -not -path "./$(FATFS_DIR)/ffunicode.c")
LWIP_SOURCES = $(wildcard $(LWIP_DIR)/core/*.c) \
               $(wildcard $(LWIP_DIR)/core/ipv4/*.c) \
               $(LWIP_DIR)/netif/ethernet.c
OBJECTS=$(ASM_SOURCES:.asm=.o) $(C_SOURCES:.c=.o)
OBJECTS += $(LWIP_SOURCES:.c=.o)

LWIP_CFLAGS = -m32 -Wa,--32 -nostdlib -nostdinc -ffreestanding \
	  -I$(PWD) $(LWIP_INCLUDES)

FATFS_SOURCES = $(FATFS_DIR)/ff.c $(FATFS_DIR)/ffunicode.c
FATFS_OBJECTS = $(FATFS_SOURCES:.c=.o)
OBJECTS += $(FATFS_OBJECTS)
FATFS_CFLAGS = -m32 -Wa,--32 -nostdlib -nostdinc -ffreestanding \
               -I$(PWD) $(FATFS_INCLUDES)

IMAGES_DST = $(BOOTLOADER_PATH)/iso
FAT_IMAGES = $(IMAGES_DST)/fat16.img $(IMAGES_DST)/fat32.img $(IMAGES_DST)/exfat.img

KERNEL          = aragveli.elf
MULTIBOOT_IMAGE	= aragveli.iso

.PHONY: all kernel initrd images cdrom run clean

all: cdrom

kernel: $(KERNEL)

images: $(FAT_IMAGES)

$(IMAGES_DST)/fat16.img:
	$(generating) '$@'
	dd if=/dev/zero of=$@ bs=1M count=16 status=none
	/usr/sbin/mkfs.fat -F 16 -n FAT16 $@ > /dev/null
	printf 'Hello from FAT16\n' > $@.txt
	mcopy -i $@ $@.txt ::/hello.txt
	mcopy -i $@ $@.txt "::/A long file name.txt"
	rm $@.txt

$(IMAGES_DST)/fat32.img:
	$(generating) '$@'
	dd if=/dev/zero of=$@ bs=1M count=40 status=none
	/usr/sbin/mkfs.fat -F 32 -s 1 -n FAT32 $@ > /dev/null
	printf 'Hello from FAT32\n' > $@.txt
	mcopy -i $@ $@.txt ::/hello.txt
	rm $@.txt
	mcopy -i $@ $(INITRD_PATH)/initrd/hello.elf ::/hello.elf

$(IMAGES_DST)/exfat.img:
	$(generating) '$@'
	dd if=/dev/zero of=$@ bs=1M count=8 status=none
	/usr/sbin/mkfs.exfat -L EXFAT $@ > /dev/null


$(KERNEL): $(OBJECTS)
	$(linking) '$^ > $@'
	$(LD) $(LDFLAGS) -T arch/x86-pc/linker.ld -o $@ $^

%.o: %.c
	$(compiling) '$< > $@'
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.asm
	$(assembling) '$< > $@'
	nasm -f elf $<

$(LWIP_DIR)/%.o: $(LWIP_DIR)/%.c
	$(CC) -c $< -o $@ $(LWIP_CFLAGS)

$(FATFS_OBJECTS): %.o: %.c
	$(compiling) '$< > $@'
	$(CC) -c $< -o $@ $(FATFS_CFLAGS)

initrd:
	$(generating) '$@'
	tar cvf initrd.tar -C $(INITRD_PATH) initrd/
	mv initrd.tar $(INITRD_DST)

cdrom: kernel initrd images
	$(generating) '$@'
	mv $(KERNEL) $(BOOTLOADER_PATH)/iso/boot/
	grub-mkrescue -o $(MULTIBOOT_IMAGE) $(BOOTLOADER_PATH)/iso/

run: cdrom
	qemu-system-i386 -m 256 -vga std \
		-netdev user,id=net0 \
		-rtc base=localtime \
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
	$(RM) $(FAT_IMAGES)
