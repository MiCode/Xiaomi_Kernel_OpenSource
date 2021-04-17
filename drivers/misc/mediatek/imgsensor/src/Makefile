# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

subdir-ccflags-y += -I$(srctree)/drivers/i2c/busses/ \
             -I$(srctree)/drivers/misc/mediatek/ccu/inc

MTK_DRIVER_PATH  := $(srctree)/drivers/misc/mediatek

# for layer decouple 2.0
ifeq ($(CONFIG_MTK_IMGSENSOR_PLATFORM_SUPPORT),y)
MTK_PLATFORM_MT6853 := mt6853
MTK_PLATFORM_MT6893 := mt6893

# for layer decouple 1.0
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_IMGSENSOR_PLATFORM))
endif

subdir-ccflags-y += \
             -I$(MTK_DRIVER_PATH)/imgsensor/inc \
             -I$(MTK_DRIVER_PATH)/cam_cal/inc

CUSTOM_KERNEL_FLAGS := $(addprefix -D, $(shell echo \
             $(CONFIG_CUSTOM_KERNEL_IMGSENSOR) | tr a-z A-Z))
subdir-ccflags-y += $(CUSTOM_KERNEL_FLAGS)

# for layer decouple 2.0
ifeq ($(CONFIG_MTK_IMGSENSOR_PLATFORM_SUPPORT),y)
obj-$(CONFIG_MTK_IMGSENSOR) += \
                        $(MTK_PLATFORM_MT6853)/ \
                        $(MTK_PLATFORM_MT6893)/

# for layer decouple 1.0
else
obj-$(CONFIG_MTK_IMGSENSOR) += \
                        $(MTK_PLATFORM)/
endif