# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

# for layer decouple 2.0
ifeq ($(CONFIG_MTK_IMGSENSOR_MT6893),m)
MTK_PLATFORM := mt6893

# for layer decouple 1.0
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_IMGSENSOR_PLATFORM))
endif

MTK_SENSOR_PROJECT :=
MTK_DERIVED_PLATFORM :=

IMGSENSOR_DRIVER_PATH := $(srctree)/drivers/misc/mediatek/imgsensor/src
COMMON_VERSION := v1_1
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf/ \
	-I$(srctree)/drivers/i2c/busses/

# for layer decouple 2.0
ifeq ($(CONFIG_MTK_IMGSENSOR_MT6893),m)
obj-$(CONFIG_MTK_IMGSENSOR_MT6893) += imgsensor_mt6893.o
imgsensor_mt6893-objs :=
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/imgsensor.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/imgsensor_hw.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/imgsensor_i2c.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/imgsensor_legacy.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/imgsensor_proc.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/seninf_clk.o
imgsensor_mt6893-objs += ../common/$(COMMON_VERSION)/seninf.o

# for layer decouple 1.0
else
obj-$(CONFIG_MTK_IMGSENSOR) += imgsensor.o
imgsensor-objs :=
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_hw.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_i2c.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_legacy.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_proc.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
imgsensor-objs += ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o
imgsensor-objs += ../common/$(COMMON_VERSION)/seninf_clk.o
imgsensor-objs += ../common/$(COMMON_VERSION)/seninf.o
endif

# for adding sensor driver
define add_subdrv
$(eval subdrv-rpath = $1)
$(info path: $(src)/$1/Makefile)
$(eval include $(src)/$1/Makefile)
endef

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(MTK_SENSOR_PROJECT)/$(1)),)
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

ifeq ($(CONFIG_MTK_IMGSENSOR_MT6893),m)
$(info imgsensor_mt6893-objs : $(imgsensor_mt6893-objs))
else
$(info imgsensor-objs : $(imgsensor-objs))
endif
$(info imgsensor subdir-ccflags-y : $(subdir-ccflags-y))