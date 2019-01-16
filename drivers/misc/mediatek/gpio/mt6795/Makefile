include $(srctree)/drivers/misc/mediatek/Makefile.custom

ifeq ($(CONFIG_MTK_FPGA),y)
obj-y	:= mt_gpio_fpga.o
else
obj-y	:= mt_gpio_base.o 
endif
obj-y	+= mt_gpio_ext.o mt_gpio_affix.o mt_gpio_debug.o mt_gpio_base_linux.o

