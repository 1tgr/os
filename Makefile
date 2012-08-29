ROOT=.
include $(ROOT)/make.conf

LIBS=arch-$(ARCH) kernel cutest

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
	qemu-system-arm -cpu arm1176 -m 256 -M versatilepb -kernel $<

qemu-i386: $(OBJDIR)/floppy.img
	qemu-system-i386 -debugcon stdio -smp 4 -fda $<

include make.actions

$(OBJDIR)/kernel.bin: linker.ld $(OBJECTS_IN_DIR) $(LIBS_IN_DIR) | $(OBJDIR)
	$(LD) -L $(ROOT)/newlib/$(TARGET)/lib -T linker.ld -o $@ $^ -lc -lnosys -lm

