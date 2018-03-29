ifneq ($(CONFIG_MTK_LCM), y)
	obj-y += mtkfb.o

	ccflags-y += \
		-I$(srctree)/drivers/misc/mediatek/video/include     \
		-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/        \
		-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys \
		-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox  \
		-I$(srctree)/drivers/misc/mediatek/sync/

endif
