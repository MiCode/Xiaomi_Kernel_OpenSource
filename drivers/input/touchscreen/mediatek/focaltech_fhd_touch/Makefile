# Makefile for the focaltech touchscreen drivers.

ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/focaltech_fhd_touch/
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/base
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/alsps/inc/

obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)   +=  focaltech_point_report_check.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)   +=  focaltech_proximity.o
# obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)   +=  focaltech_test/

obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)   +=  focaltech_i2c.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_flash.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)   +=  focaltech_flash/
