all: kernel.bin

clean:
	rm loader.o isr.o kernel.o kernel.bin

.S.o:
	i386-elf-gcc-4.3.2 -o $@ -c $<

.c.o:
	i386-elf-gcc-4.3.2 -Wall -Wextra -Werror -fno-builtin -nostartfiles -nodefaultlibs -std=c99 -I newlib/i386-elf/include -o $@ -c $<

kernel.bin: loader.o isr.o kernel.o linker.ld
	i386-elf-ld -L newlib/i386-elf/lib -T linker.ld -o $@ $^ -lc -lnosys
