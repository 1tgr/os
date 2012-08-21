CC=i386-elf-gcc-4.3.2
LD=i386-elf-ld
CFLAGS=-g -O2 -march=i486 -Wall -Wextra -Werror -Wno-unused-parameter -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -std=c99 -U __unix__ -D __DYNAMIC_REENT__ -I newlib/i386-elf/include
OBJDIR=obj
OBJECTS=loader.o isr.o array.o inbox.o interrupt.o kernel.o lock.o obj.o test.o thread.o cutest/CuTest.o
OBJECTS_IN_DIR=$(addprefix $(OBJDIR)/, $(OBJECTS))

all: obj/kernel.bin

clean:
	rm obj/floppy.img obj/kernel.bin $(OBJECTS_IN_DIR) $(OBJECTS_IN_DIR:.o=.d)

$(OBJDIR)/%.d: %.S | $(OBJDIR)
	printf $(OBJDIR)/ > $@
	$(CC) $(CFLAGS) $(CPPFLAGS) -M -o - $< >> $@ || rm $@

$(OBJDIR)/%.d: %.c | $(OBJDIR) $(OBJDIR)/cutest
	printf $(OBJDIR)/ > $@
	$(CC) $(CFLAGS) $(CPPFLAGS) -M -o - $< >> $@ || rm $@

$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: %.c | $(OBJDIR) $(OBJDIR)/cutest
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/cutest:
	mkdir $(OBJDIR)/cutest

obj/kernel.bin: linker.ld $(OBJECTS_IN_DIR)
	$(LD) -L newlib/i386-elf/lib -T linker.ld -o $@ $^ -lc -lnosys -lm

grub.img.gz: stage1 stage2
	dd if=/dev/zero of=obj/grub.img bs=512 count=2880
	mkfs.vfat obj/grub.img
	mcopy -i obj/grub.img $^ ::/
	printf "device (fd0) obj/grub.img\nroot (fd0)\ninstall /stage1 (fd0) /stage2 /menu.lst\n" | grub --batch
	gzip -c obj/grub.img > $@

obj/floppy.img: grub.img.gz obj/kernel.bin menu.lst
	gzip -dc $< > $@
	mcopy -i $@ obj/kernel.bin menu.lst ::/

qemu: obj/floppy.img
	qemu-system-i386 -debugcon stdio -smp 4 -fda $<

include $(OBJECTS_IN_DIR:.o=.d)
