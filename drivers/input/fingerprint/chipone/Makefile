ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)

#just for beanpod sdk compatible
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/400/tz_driver/include
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/400/common/include/
subdir-ccflags-y += -I$(srctree)/drivers/tee/teei/400/tz_vfs/

obj-y += fpsensor_spi_tee.o

