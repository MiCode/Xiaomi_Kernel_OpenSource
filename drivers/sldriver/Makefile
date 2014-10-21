obj-m := vidt_driver.o

ifeq ($(ARCH) , x86_64)
vidt_driver-objs := vmm_hsym.o kern_ta.o vidt.o isr64.o
endif

obj-m += elf_i386.lds
ccflags-y += -Idrivers/sldriver/
ccflags-y += -fno-pic
ccflags-y += -Wno-error=strict-prototypes
