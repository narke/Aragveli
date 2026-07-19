CC      = gcc
CFLAGS  = -m32 -c -ffreestanding -nostdlib -fno-builtin
NASM    = nasm
LD      = ld

LIBC_SRCS = libc/stdio.c libc/stdlib.c libc/string.c
LIBC_OBJS = $(LIBC_SRCS:.c=.o)

COMMON = start.o $(LIBC_OBJS)

.PHONY: all clean

all: hello.elf shell.elf

hello.elf: hello.o $(COMMON) user.ld
	$(LD) -m elf_i386 -nostdlib -static -T user.ld -o hello.elf hello.o $(COMMON)
	cp hello.elf ../extra/initrd/

shell.elf: shell.o $(COMMON) user.ld
	$(LD) -m elf_i386 -nostdlib -static -T user.ld -o shell.elf shell.o $(COMMON)
	cp shell.elf ../extra/initrd/

hello.o: hello.c
	$(CC) $(CFLAGS) -Ilibc hello.c -o hello.o

shell.o: shell.c
	$(CC) $(CFLAGS) -Ilibc shell.c -o shell.o

start.o: start.asm
	$(NASM) -f elf32 start.asm -o start.o

libc/%.o: libc/%.c
	$(CC) $(CFLAGS) -Ilibc libc/$*.c -o libc/$*.o

clean:
	$(RM) hello.elf shell.elf start.o hello.o shell.o $(LIBC_OBJS)
