CC      = gcc
CFLAGS  = -m32 -c -ffreestanding -nostdlib -fno-builtin
NASM    = nasm
LD      = ld

LIBC_SRCS = libc/stdio.c libc/stdlib.c libc/string.c
LIBC_OBJS = $(LIBC_SRCS:.c=.o)

OBJS = start.o hello.o $(LIBC_OBJS)

hello.elf: $(OBJS) user.ld
	$(LD) -m elf_i386 -nostdlib -static -T user.ld -o hello.elf $(OBJS)
	cp hello.elf ../extra/initrd/

hello.o: hello.c
	$(CC) $(CFLAGS) -Ilibc hello.c -o hello.o

start.o: start.asm
	$(NASM) -f elf32 start.asm -o start.o

libc/%.o: libc/%.c
	$(CC) $(CFLAGS) -Ilibc libc/$*.c -o libc/$*.o

clean:
	$(RM) hello.elf start.o hello.o $(LIBC_OBJS)
