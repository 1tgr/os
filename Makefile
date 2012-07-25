CC=i386-elf-gcc-4.3.2
CFLAGS=-Wall -Wextra -Werror -Wno-unused-parameter -fno-builtin -nostartfiles -nodefaultlibs -std=c99 -I newlib/i386-elf/include

all: kernel.bin

clean:
	rm loader.o isr.o kernel.o kernel.bin

kernel.bin: linker.ld loader.o isr.o array.o inbox.o kernel.o obj.o test.o thread.o cutest/CuTest.o
	i386-elf-ld -L newlib/i386-elf/lib -T linker.ld -o $@ $^ -lc -lnosys -lm
