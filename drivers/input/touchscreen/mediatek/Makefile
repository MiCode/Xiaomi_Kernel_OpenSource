#subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
# In case the platform does NOT support this type of sensors
obj-y   +=  mtk_tpd.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_calibrate.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
obj-$(CONFIG_TOUCHSCREEN_MTK_SOLOMON)	+=  SOLOMON/
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_touch/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT1151)	+=  GT1151/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT911)	+=  GT911/
obj-$(CONFIG_TOUCHSCREEN_MTK_GSLX680)	+=  gslX680/
obj-$(CONFIG_TOUCHSCREEN_MTK_NT36xxx)	+=  NT36xxx/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XX_LM)	+=  GT9XX_LM/
obj-$(CONFIG_TOUCHSCREEN_FT5X26)	+=  FT5X26/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT5688)	+=  GT5688/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_I2C)	+=  hxchipset/
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_fhd_touch/

obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI)	+=  NT36xxx_HOSTDL_SPI/
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_touch/
obj-$(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE)	+=  xiaomi/
obj-$(CONFIG_TOUCHSCREEN_MTK_NT36672)	+=  NT36672/
obj-$(CONFIG_TOUCHSCREEN_MTK_FT8719P)	+=  FT8719P/

obj-$(CONFIG_TOUCHSCREEN_MTK_GT9157)    +=  GT9157/

obj-$(CONFIG_TOUCHSCREEN_NT36672C)      += NT36672C/
# Huaqin add for HQ-147450 by feiwen at 2021/7/23 start
obj-$(CONFIG_TOUCHSCREEN_FTS_XIAOMI)	+=  focaltech_touch_k19a/
# Huaqin add for HQ-147450 by feiwen at 2021/7/23 end