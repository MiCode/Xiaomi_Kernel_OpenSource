# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

MTK_PLATFORM := isp6s

MTK_SENSOR_PROJECT :=
MTK_DERIVED_PLATFORM :=


IMGSENSOR_DRIVER_PATH := $(srctree)/drivers/misc/mediatek/imgsensor/src
COMMON_VERSION := v1_1
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf/ \
	-I$(srctree)/drivers/i2c/busses/

obj-$(CONFIG_MTK_IMGSENSOR) += imgsensor_isp6s.o
imgsensor_isp6s-objs :=
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/imgsensor.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/imgsensor_hw.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/imgsensor_i2c.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/imgsensor_legacy.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/imgsensor_proc.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/seninf_clk.o
imgsensor_isp6s-objs += ../common/$(COMMON_VERSION)/seninf.o

IMG_COMMON_CODE_PATH := ../common/$(COMMON_VERSION)
include $(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/n3d_fsync/Makefile

# for adding sensor driver
define add_subdrv
$(eval subdrv-rpath = $1)
$(info path: $(src)/$1/Makefile)
$(eval include $(src)/$1/Makefile)
endef

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/$(1)),)
unknown_drv += $(1)
else
common_drv += $(1)
endif
else
platform_drv += $(1)
endif
else
project_drv += $(1)
endif
endef

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))

ifneq ($(unknown_drv),)
$(info unknown_drv: $(unknown_drv))
endif

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
$(foreach drv, $(project_drv), $(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/$(drv))))
endif

ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw_$(MTK_DERIVED_PLATFORM)))
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
$(eval $(call add_subdrv,camera_hw))
else
$(eval $(call add_subdrv,camera_hw_$(MTK_DERIVED_PLATFORM)))
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw_$(MTK_DERIVED_PLATFORM)
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
endif
ifneq ($(project_drv),)
$(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
endif
else
$(eval $(call add_subdrv,camera_project/$(MTK_SENSOR_PROJECT)/camera_hw))
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/camera_hw
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw_$(MTK_DERIVED_PLATFORM)
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
endif

subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/camera_hw

## Enable parallelism
# subdir-ccflags-y += -DSENSOR_PARALLEISM

ifneq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf_$(MTK_DERIVED_PLATFORM)))
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf_$(MTK_DERIVED_PLATFORM)/
$(eval $(call add_subdrv,seninf_$(MTK_DERIVED_PLATFORM)))
else
$(eval $(call add_subdrv,seninf))
endif

subdir-ccflags-y += -DNO_I2C_MTK -DNO_CLK_METER -DSENINF_USE_RPM -DDFS_CTRL_BY_OPP
subdir-ccflags-y += -DSENINF_N3D_SUPPORT

$(info imgsensor_isp6s-objs : $(imgsensor_isp6s-objs))
$(info imgsensor_isp6s subdir-ccflags-y : $(subdir-ccflags-y))
