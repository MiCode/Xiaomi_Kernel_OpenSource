obj-$(CONFIG_MTK_USBFSH) := musbfsh_host.o musbfsh_hsdma.o musbfsh_virthub.o
obj-$(CONFIG_MTK_ICUSB_SUPPORT) += musbfsh_icusb.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb11/$(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-y += -I$(srctree)/drivers/usb/core/
subdir-ccflags-$(CONFIG_MTK_MUSBFSH_QMU_SUPPORT) += -DMUSBFSH_QMU_SUPPORT
ifeq ($(CONFIG_MTK_MUSBFSH_QMU_SUPPORT),y)
subdir-ccflags-$(CONFIG_ARCH_MT8163) += -DMUSBFSH_QMU_LIMIT_SUPPORT
endif
obj-$(CONFIG_ARCH_MT6755) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT6797) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8163) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_ARCH_MT8167) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

# QMU Realted
obj-$(CONFIG_MTK_MUSBFSH_QMU_SUPPORT) += mtk11_qmu.o musbfsh_qmu.o
