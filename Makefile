loader.o: loader.s
	i386-elf-as -o loader.o loader.s

kernel.o: kernel.c
	i386-elf-gcc-4.3.2 -o kernel.o -c kernel.c -Wall -Wextra -Werror -nostdlib -fno-builtin -nostartfiles -nodefaultlibs

kernel.bin: loader.o kernel.o linker.ld
	i386-elf-ld -T linker.ld -o kernel.bin loader.o kernel.o
