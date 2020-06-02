# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
IMGSENSOR_DRIVER_PATH  := $(srctree)/drivers/misc/mediatek/imgsensor/src
COMMON_VERSION := v1_1
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf/ \
	-I$(srctree)/drivers/i2c/busses/

obj-$(CONFIG_MTK_IMGSENSOR) += imgsensor.o
imgsensor-objs :=

imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_hw.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_i2c.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_legacy.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_proc.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o
imgsensor-objs += ../common/$(COMMON_VERSION)/seninf.o

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/$(1)),)
common_drv += $(1)
else
platform_drv += $(1)
endif
else
project_drv += $(1)
endif
endef

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))

define add_subdrv
$(eval subdrv-rpath = $1)
$(eval include $(src)/$1/subdrv.mk)
endef

ifneq ($(common_drv),)
$(info common_drv : $(common_drv))
$(foreach drv,$(common_drv),$(eval $(call add_subdrv,../common/$(COMMON_VERSION)/$(drv))))
endif

ifneq ($(platform_drv),)
$(info platform_drv : $(platform_drv))
$(foreach drv,$(platform_drv),$(eval $(call add_subdrv,$(drv))))
endif

ifneq ($(project_drv),)
$(info project_drv : $(project_drv))
$(foreach drv, $(project_drv), $(eval $(call add_subdrv,camera_project/$(ARCH_MTK_PROJECT)/$(drv))))
endif

ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw))
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
$(eval $(call add_subdrv,camera_hw))
else
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw
$(eval $(call add_subdrv,camera_project/$(ARCH_MTK_PROJECT)/camera_hw))
endif

$(eval $(call add_subdrv,seninf))

subdir-ccflags-y += -DNO_I2C_MTK -DNO_CLK_METER -DSENINF_USE_RPM #-DDFS_CTRL_BY_OPP

$(info imgsensor-objs : $(imgsensor-objs))
$(info subdir-ccflags-y : $(subdir-ccflags-y))
