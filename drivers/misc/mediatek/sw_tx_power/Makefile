include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(srctree)/drivers/misc/mediatek/sw_tx_power/$(subst ",,$(CONFIG_MTK_PLATFORM))/

obj-y += sw_tx_power_eint.o
obj-y += sw_tx_power_sysfs.o

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
