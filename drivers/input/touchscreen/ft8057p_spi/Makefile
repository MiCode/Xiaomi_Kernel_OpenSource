# Makefile for the focaltech touchscreen drivers.


ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/misc/mediatek/scp/include/

obj-m += ft8057p_spi.o
ft8057p_spi-y += focaltech_core.o
ft8057p_spi-y += focaltech_ex_fun.o
ft8057p_spi-y += focaltech_ex_mode.o
ft8057p_spi-y += focaltech_gesture.o
ft8057p_spi-y += focaltech_esdcheck.o
ft8057p_spi-y += focaltech_point_report_check.o
ft8057p_spi-y += focaltech_flash_spi.o
ft8057p_spi-y += focaltech_spi_v2.o
ft8057p_spi-y += focaltech_proximity.o
ft8057p_spi-y += focaltech_test/focaltech_test.o
ft8057p_spi-y += focaltech_test/focaltech_test_ini.o
ft8057p_spi-y += focaltech_test/supported_ic/focaltech_test_ft8057p.o
