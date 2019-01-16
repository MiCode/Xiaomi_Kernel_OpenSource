include $(srctree)/drivers/misc/mediatek/Makefile.custom

obj-$(CONFIG_MTK_USBFSH) := musbfsh_host.o musbfsh_hsdma.o musbfsh_virthub.o
obj-$(CONFIG_MTK_ICUSB_SUPPORT) += musbfsh_icusb.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb11/$(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-y += -I$(srctree)/drivers/usb/core/

obj-$(CONFIG_ARCH_MT6595) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6752) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6795) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8127) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
