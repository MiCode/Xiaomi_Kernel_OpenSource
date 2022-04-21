# SPDX-License-Identifier: GPL-2.0

MTK_CUSTOM_PATH := $(srctree)/drivers/misc/mediatek
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
CAM_CAL_SRC_PATH := $(MTK_CUSTOM_PATH)/cam_cal/src
CAM_CAL_PLATFORM_SRC_PATH := $(CAM_CAL_SRC_PATH)/$(MTK_PLATFORM)
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
COMMON_VERSION := v1

ifeq ($(CONFIG_ARCH_MTK_PROJECT), "tb8786p2_64_k419_wifi")
KBUILD_CFLAGS += -DSUPPORT_S5K4H7
endif

subdir-ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \
                    -I$(MTK_CUSTOM_PATH)/imgsensor/src/$(CONFIG_MTK_PLATFORM)/camera_hw \
                    -I$(MTK_CUSTOM_PATH)/cam_cal/inc \
                    -I$(CAM_CAL_PLATFORM_SRC_PATH) \
                    -I$(CAM_CAL_SRC_PATH)/common/$(COMMON_VERSION)
subdir-ccflags-y += -I$(srctree)/drivers/i2c/busses/

# Common code
obj-y += ../common/$(COMMON_VERSION)/eeprom_driver.o
obj-y += ../common/$(COMMON_VERSION)/eeprom_i2c_common_driver.o

# Platform code
obj-y += eeprom_i2c_custom_driver.o


define INC_CUSTOM_SRC
ifeq ($(wildcard $(CAM_CAL_PLATFORM_SRC_PATH)/camera_project/$(ARCH_MTK_PROJECT)/$(1).c),)
obj-y += $(1).o
else
obj-y += camera_project/$(ARCH_MTK_PROJECT)/$(1).o
endif
endef

# Project or platform code
CUSTOM_SRC_LIST := cam_cal_list eeprom_i2c_dev
$(foreach src,$(CUSTOM_SRC_LIST),$(eval $(call INC_CUSTOM_SRC,$(src))))
