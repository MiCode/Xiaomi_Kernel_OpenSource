#include $(srctree)/drivers/misc/mediatek/Makefile.custom

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

MTK_PLATFORM_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)
MTK_PROJECT_PATH_ROOT := $(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)

ccflags-y += -I$(MTK_PROJECT_PATH_ROOT)/camera/camera \

MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

ccflags-y += -I$(MTK_CUSTOM_PATH)/camera/$(MTK_PLATFORM) \
             -I$(MTK_CUSTOM_PATH)/i2c/$(MTK_PLATFORM) \
             -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
             -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(MTK_PLATFORM)/camera_hw \

obj-y += hm2051mipiraw_Sensor.o


