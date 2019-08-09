subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/

obj-$(CONFIG_MTK_FREQ_HOPPING) := mt_freqhopping_drv.o

ifeq ($(CONFIG_MTK2712_FREQ_HOPPING),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/mt2712/
obj-y += mt2712/
endif

ifeq ($(CONFIG_MTK8173_FREQ_HOPPING),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/mt8173/
obj-y += mt8173/
endif

ifeq ($(CONFIG_MTK8163_FREQ_HOPPING),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/mt8163/
obj-y += mt8163/
endif

ifeq ($(CONFIG_MTK6758_FREQ_HOPPING),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/mt6758/
obj-y += mt6758/
endif

ifeq ($(CONFIG_MTK6765_FREQ_HOPPING),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/mt6765/
obj-y += mt6765/
endif


subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/freqhopping/$(MTK_PLATFORM)/
obj-y += $(MTK_PLATFORM)/
