#For display ESD recovery
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

obj-$(CONFIG_TOUCHSCREEN_GOODIX_BRL) += gt9966.o
gt9966-y := \
				goodix_brl_i2c.o \
				goodix_brl_spi.o \
				goodix_ts_core.o \
				goodix_brl_hw.o \
				goodix_cfg_bin.o \
				goodix_ts_utils.o \
				goodix_brl_fwupdate.o \
				goodix_ts_gesture.o \
				goodix_ts_inspect.o \
				goodix_ts_tools.o
