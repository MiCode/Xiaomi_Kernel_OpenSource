#
# Makefile for the Goodix GT9886 touchscreen driver.
#

#For display ESD recovery
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-m += gt9886.o
gt9886-y += goodix_ts_i2c.o
gt9886-y += goodix_ts_core.o
gt9886-y += goodix_cfg_bin.o
gt9886-y += goodix_gtx8_update.o
gt9886-y += goodix_ts_tui.o
