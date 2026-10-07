CC      = gcc
CFLAGS  = -m32 -c -ffreestanding -nostdlib -fno-builtin -Wformat -Wformat-security
NASM    = nasm
LD      = ld

LIBC_SRCS = libc/stdio.c libc/stdlib.c libc/string.c
LIBC_OBJS = $(LIBC_SRCS:.c=.o)

COMMON = start.o $(LIBC_OBJS)

.PHONY: all clean

all: hello.elf shell.elf lspci.elf

hello.elf: hello.o $(COMMON) user.ld
	$(LD) -m elf_i386 -nostdlib -static -T user.ld -o hello.elf hello.o $(COMMON)
	mkdir -p ../initrd
	cp hello.elf ../initrd/

shell.elf: shell.o $(COMMON) user.ld
	$(LD) -m elf_i386 -nostdlib -static -T user.ld -o shell.elf shell.o $(COMMON)
	mkdir -p ../initrd
	cp shell.elf ../initrd/

lspci.elf: lspci.o $(COMMON) user.ld
	$(LD) -m elf_i386 -nostdlib -static -T user.ld -o lspci.elf lspci.o $(COMMON)
	mkdir -p ../initrd
	cp lspci.elf ../initrd/

hello.o: hello.c
	$(CC) $(CFLAGS) -Ilibc hello.c -o hello.o

shell.o: shell.c
	$(CC) $(CFLAGS) -Ilibc shell.c -o shell.o

lspci.o: lspci.c
	$(CC) $(CFLAGS) -Ilibc lspci.c -o lspci.o

start.o: start.asm
	$(NASM) -f elf32 start.asm -o start.o

libc/%.o: libc/%.c
	$(CC) $(CFLAGS) -Ilibc libc/$*.c -o libc/$*.o

clean:
	$(RM) hello.elf shell.elf lspci.elf start.o hello.o shell.o lspci.o $(LIBC_OBJS)
