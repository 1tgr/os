CC=i386-elf-gcc-4.3.2
LD=i386-elf-ld
CFLAGS=-g -O2 -Wall -Wextra -Werror -Wno-unused-parameter -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -std=c99 -U __unix__ -I newlib/i386-elf/include
OBJDIR=obj
OBJECTS=loader.o isr.o array.o inbox.o kernel.o lock.o obj.o test.o thread.o cutest/CuTest.o
OBJECTS_IN_DIR=$(addprefix $(OBJDIR)/, $(OBJECTS))

all: kernel.bin

clean:
	rm kernel.bin $(OBJECTS_IN_DIR) $(OBJECTS_IN_DIR:.o=.d)

$(OBJDIR)/%.d: %.S | $(OBJDIR)
	printf $(OBJDIR)/ > $@
	$(CC) $(CFLAGS) $(CPPFLAGS) -M -o - $< >> $@

$(OBJDIR)/%.d: %.c | $(OBJDIR) $(OBJDIR)/cutest
	printf $(OBJDIR)/ > $@
	$(CC) $(CFLAGS) $(CPPFLAGS) -M -o - $< >> $@

$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c | $(OBJDIR) $(OBJDIR)/cutest
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/cutest:
	mkdir $(OBJDIR)/cutest

kernel.bin: linker.ld $(OBJECTS_IN_DIR)
	$(LD) -L newlib/i386-elf/lib -T linker.ld -o $@ $^ -lc -lnosys -lm

qemu: kernel.bin
	qemu-system-i386 -kernel $<

include $(OBJECTS_IN_DIR:.o=.d)
