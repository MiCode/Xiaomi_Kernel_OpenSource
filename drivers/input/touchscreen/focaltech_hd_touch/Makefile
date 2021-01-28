# Makefile for the focaltech touchscreen drivers.

ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/focaltech_hd_touch/
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/base
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/alsps/inc/

obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)   +=  focaltech_point_report_check.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)   +=  focaltech_proximity.o
# obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)   +=  focaltech_test/

obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)   +=  focaltech_i2c.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)	+=  focaltech_flash.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_HD)   +=  focaltech_flash/

obj-y   +=  tpd_control.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
