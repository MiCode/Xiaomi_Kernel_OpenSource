ccflags-y := -I$(srctree)/drivers/misc/mediatek/include
ccflags-$(CONFIG_MTK_PRINTK_UART_CONSOLE) += -I$(srctree)/drivers/misc/mediatek/mtprintk/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/uart

obj-y	= printk.o
obj-$(CONFIG_PRINTK_NMI)		+= nmi.o
obj-$(CONFIG_A11Y_BRAILLE_CONSOLE)	+= braille.o
