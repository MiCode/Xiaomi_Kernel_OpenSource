subdir-ccflags-$(CONFIG_MACH_MT8167) += -DMUSBFSH_ENABLE_BUS_SUSPEND

obj-$(CONFIG_MTK_USBFSH) := musbfsh_host.o musbfsh_hsdma.o musbfsh_virthub.o
obj-$(CONFIG_MTK_ICUSB_SUPPORT) += musbfsh_icusb.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/usb11/$(subst ",,$(CONFIG_MTK_PLATFORM))/
ccflags-y += -I$(srctree)/drivers/usb/core/

obj-$(CONFIG_MACH_MT8167) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

# QMU Realted
obj-$(CONFIG_MTK_MUSBFSH_QMU_SUPPORT) += mtk11_qmu.o musbfsh_qmu.o
ifeq ($(CONFIG_MTK_MUSBFSH_QMU_SUPPORT),y)
subdir-ccflags-$(CONFIG_ARCH_MT8163) += -DMUSBFSH_QMU_LIMIT_SUPPORT -DMUSBFSH_QMU_LIMIT_RXQ_NUM=4 -DMUSBFSH_QMU_LIMIT_TXQ_NUM=4
endif
ifeq ($(CONFIG_MTK_MUSBFSH_QMU_SUPPORT),y)
subdir-ccflags-$(CONFIG_MACH_MT8167) += -DMUSBFSH_QMU_LIMIT_SUPPORT -DMUSBFSH_QMU_LIMIT_RXQ_NUM=4 -DMUSBFSH_QMU_LIMIT_TXQ_NUM=4
endif