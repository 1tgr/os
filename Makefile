CC=i386-elf-gcc-4.3.2
CFLAGS=-Wall -Wextra -Werror -Wno-unused-parameter -fno-builtin -nostartfiles -nodefaultlibs -std=c99 -I newlib/i386-elf/include
OBJDIR=obj
OBJECTS=loader.o isr.o array.o inbox.o kernel.o obj.o test.o thread.o cutest/CuTest.o

all: kernel.bin

clean:
	rm kernel.bin $(addprefix $(OBJDIR)/, $(OBJECTS))

$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c | $(OBJDIR) $(OBJDIR)/cutest
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/cutest:
	mkdir $(OBJDIR)/cutest

kernel.bin: linker.ld $(addprefix $(OBJDIR)/, $(OBJECTS))
	i386-elf-ld -L newlib/i386-elf/lib -T linker.ld -o $@ $^ -lc -lnosys -lm

qemu: kernel.bin
	qemu-system-i386 -kernel $<
