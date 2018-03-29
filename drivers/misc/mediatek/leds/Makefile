
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include

obj-$(CONFIG_MTK_LEDS) += leds_drv.o

ifneq ($(filter y,$(CONFIG_ARCH_MT8127) $(CONFIG_ARCH_MT7623) $(CONFIG_ARCH_MT8590)),)
CONFIG_MTK_PLATFORM := mt8127
ccflags-y += -I$(srctree)/drivers/misc/mediatek/leds/$(CONFIG_MTK_PLATFORM)
endif
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

