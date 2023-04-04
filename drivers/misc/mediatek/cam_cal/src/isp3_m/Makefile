# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := isp3_m
MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_PLATFORM_SRC_PATH := $(CAM_CAL_SRC_PATH)/$(MTK_PLATFORM)
COMMON_VERSION := v1
MTK_PLATFORM_KO := camera_eeprom_$(MTK_PLATFORM)

CAM_CAL_OBJ := \
	../common/$(COMMON_VERSION)/eeprom_driver.o \
	../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o \
	eeprom_i2c_custom_driver.o \
	cam_cal_list.o \
	eeprom_i2c_dev.o \

ifeq ($(CONFIG_MTK_CAM_CAL_ISP3_M), m)
obj-$(CONFIG_MTK_CAM_CAL_ISP3_M) += $(MTK_PLATFORM_KO).o
$(eval $(MTK_PLATFORM_KO)-objs += $(CAM_CAL_OBJ))
else ifeq ($(CONFIG_MTK_CAM_CAL_ISP3_M), y)
obj-$(CONFIG_MTK_CAM_CAL_ISP3_M) += $(CAM_CAL_OBJ)
endif

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(MTK_PLATFORM)/camera_hw \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_PLATFORM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION) \
                    -I$(srctree)/drivers/i2c/busses/
