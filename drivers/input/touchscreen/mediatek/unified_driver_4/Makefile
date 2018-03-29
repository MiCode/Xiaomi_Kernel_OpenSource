subdir-ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -Werror -I$(srctree)/drivers/input/touchscreen/mediatek/unified_driver_4/inc

obj-y	+=  lgtp_common_driver.o
obj-y	+=  lgtp_model_config_i2c.o
obj-y	+=  lgtp_model_config_misc.o
obj-y	+=  lgtp_platform_api_i2c.o
obj-y	+=  lgtp_platform_api_misc.o
obj-y	+=  lgtp_platform_api_power.o

obj-$(CONFIG_TOUCHSCREEN_LGE_MIT300) += MIT300/
obj-$(CONFIG_TOUCHSCREEN_LGE_MIT300_M4) += MIT300/
obj-$(CONFIG_TOUCHSCREEN_LGE_FT8707) += FT8707/