# Goodix Touchscreen Makefile
# This Makefile is only for tempory compiling use
# Use xxxproject/Makefile-release when release driver
#
# core layer
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/

obj-y += goodix_ts_i2c.o
obj-y += goodix_ts_core.o
obj-y += goodix_cfg_bin.o
obj-y += goodix_gtx8_update.o
obj-n += goodix_ts_tui.o
