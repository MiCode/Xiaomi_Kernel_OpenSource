include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_SERIAL) := uart.o
obj-$(CONFIG_FIQ_DEBUGGER) += mt_fiq_debugger.o

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

