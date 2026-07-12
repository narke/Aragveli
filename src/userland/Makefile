hello.elf: hello.c start.asm user.ld
	gcc -m32 -c -ffreestanding -nostdlib -fno-builtin hello.c -o hello.o
	nasm -f elf32 start.asm -o start.o
	ld -m elf_i386 -nostdlib -static -T user.ld -o hello.elf start.o hello.o
	cp hello.elf ../extra/initrd/
