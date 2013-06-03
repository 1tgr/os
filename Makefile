ROOT=.
include $(ROOT)/make.conf

LIBS=arch-$(ARCH) kernel cutest
LIBPATH=$(ROOT)/newlib/$(TARGET)/lib

all: $(OBJDIR)/kernel.bin

grub.img.gz: stage1 stage2
	dd if=/dev/zero of=$(OBJDIR)/grub.img bs=512 count=2880
	mkfs.vfat $(OBJDIR)/grub.img
	mcopy -i $(OBJDIR)/grub.img $^ ::/
	printf "device (fd0) $(OBJDIR)/grub.img\nroot (fd0)\ninstall /stage1 (fd0) /stage2 /menu.lst\n" | grub --batch
	gzip -c $(OBJDIR)/grub.img > $@

$(OBJDIR)/floppy.img: grub.img.gz $(OBJDIR)/kernel.bin menu.lst
	gzip -dc $< > $@
	mcopy -i $@ $(OBJDIR)/kernel.bin menu.lst ::/

qemu: qemu-$(ARCH)

qemu-arm: $(OBJDIR)/kernel.bin
	qemu-system-arm -monitor stdio -cpu arm1176 -m 256 -M versatilepb -kernel $<

qemu-i386: $(OBJDIR)/floppy.img
	qemu-system-i386 -debugcon stdio -smp 4 -fda $<

include make.actions

LINKER+=-lnosys

$(OBJDIR)/kernel.bin: linker.ld $(OBJECTS_IN_DIR) $(LIBS_IN_DIR) | $(OBJDIR)
	$(LD) -L $(LIBPATH) -T linker.ld -o $@ $(OBJECTS_IN_DIR) $(LIBS_IN_DIR) $(LINKER) $(shell $(CC) -print-libgcc-file-name)

$(OBJDIR)/kernel.txt: $(OBJDIR)/kernel.bin
	$(OBJDUMP) -S $< > $@

