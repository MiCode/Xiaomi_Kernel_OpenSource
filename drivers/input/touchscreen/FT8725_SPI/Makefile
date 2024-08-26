# Makefile for the focaltech touchscreen drivers.
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
# N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 start
ccflags-y += -I$(srctree)/drivers/input/touchscreen/tpd_notify/
# N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 end
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/base
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/alsps/inc/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sensors-1.0/alsps/inc/
# N19A code for HQ-353617 by p-huangyunbiao at 2024/02/19 start
ccflags-y += -I$(srctree)/drivers/
# N19A code for HQ-353617 by p-huangyunbiao at 2024/02/19 end

# N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 start
obj-$(CONFIG_TOUCHSCREEN_FTS_SPI) += focaltech_tp.o
# N19A code for HQ-359934 by p-huangyunbiao at 2023/12/19 end
focaltech_tp-y := focaltech_core.o \
		focaltech_ex_fun.o \
		focaltech_ex_mode.o \
		focaltech_gesture.o \
		focaltech_esdcheck.o \
		focaltech_point_report_check.o \
		focaltech_flash.o \
		focaltech_proximity.o \
		focaltech_spi.o

focaltech_tp-y += focaltech_test/focaltech_test.o
focaltech_tp-y += focaltech_test/focaltech_test_ini.o
focaltech_tp-y += focaltech_test/supported_ic/focaltech_test_ft8725.o
