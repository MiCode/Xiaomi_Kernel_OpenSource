# drivers/image/jpeg/Makefile
include $(srctree)/drivers/misc/mediatek/Makefile.custom

EXTRA_CFLAGS = -DSDEBUG_T

#ccflags-y += -I$(MTK_PATH_SOURCE)/drivers/jpeg/
ccflags-y += \
    -I$(srctree)/drivers/misc/mediatek/cmdq/$(MTK_PLATFORM)

#obj-$(CONFIG_MTK_JPEG) += jpeg_driver.o
obj-y += jpeg_driver.o

jpeg_driver-objs := jpeg_cmdq.o jpeg_drv_6589_enc.o jpeg_drv_6589_dec.o jpeg_drv.o

# EOF
