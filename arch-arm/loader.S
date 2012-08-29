.global loader
.set STACKSIZE, 0x4000
.lcomm stack, STACKSIZE

loader:
    ldr sp, =(stack + STACKSIZE)
    bl kmain

hang:
    b hang

