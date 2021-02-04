
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# beanpod v2 compatible
ccflags-y += -I$(srctree)/drivers/misc/mediatek/teei/V1.0/tz_vfs

# TEE support
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/tkcore/core

# sunwave driver file
obj-y          := sf_ctl.o sf_hw.o

