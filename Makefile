all: kernel.bin

clean:
	rm loader.o kernel.o kernel.bin

loader.o: loader.s
	i386-elf-as -o loader.o loader.s

kernel.o: kernel.c
	i386-elf-gcc-4.3.2 -Wall -Wextra -Werror -fno-builtin -nostartfiles -nodefaultlibs -std=c99 -I newlib/i386-elf/include -o kernel.o -c kernel.c 

kernel.bin: loader.o kernel.o linker.ld
	i386-elf-ld -L newlib/i386-elf/lib -T linker.ld -o kernel.bin loader.o kernel.o -lc -lnosys
