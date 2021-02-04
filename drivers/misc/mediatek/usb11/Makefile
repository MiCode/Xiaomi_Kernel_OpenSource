ifeq ($(MTK_ALPS_BOX_SUPPORT),yes)
ccflags-y += -DMTK_ALPS_BOX_SUPPORT
endif

obj-$(CONFIG_MTK_USBFSH) := musbfsh_host.o musbfsh_hsdma.o musbfsh_virthub.o
obj-$(CONFIG_MTK_USBFSH) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb20
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb11
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb11/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/include/linux/

