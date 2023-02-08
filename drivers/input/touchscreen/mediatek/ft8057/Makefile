# Makefile for the focaltech touchscreen drivers.
# C3T code for HQ-218218 by chenzimo at 2022/8/09 start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/alsps/inc

obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_point_report_check.o
# obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_proximity.o
obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_test/



obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_flash.o



obj-$(CONFIG_TOUCHSCREEN_FTS)   +=  focaltech_spi.o
# C3T code for HQ-218218 by chenzimo at 2022/8/09 end
