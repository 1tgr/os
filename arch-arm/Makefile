include ../make.conf

OBJECTS=loader.o init.o font0.o

all: $(OBJDIR)/libarch-arm.a

obj/arm-elf/font0.o: font0.bin
	arm-elf-objcopy -B arm -I binary -O elf32-littlearm $< $@

obj/arm-elf/font0.d: | $(OBJDIR)
	touch $@

include ../make.actions
