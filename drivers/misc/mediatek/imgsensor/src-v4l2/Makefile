# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

MKFILE_PATH := $(lastword $(MAKEFILE_LIST))
IMGSENSOR_DRIVER_PATH := $(patsubst %/,%,$(dir $(realpath $(MKFILE_PATH))))
MTK_DRIVER_PATH := $(srctree)/drivers/misc/mediatek

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

subdir-ccflags-y += \
	-I$(MTK_DRIVER_PATH)/imgsensor/inc \
	-I$(IMGSENSOR_DRIVER_PATH)/ \
	-I$(srctree)/drivers/i2c/busses \

imgsensor-objs := \
	adaptor-hw.o \
	adaptor-drv.o \
	adaptor-i2c.o \
	adaptor-ctrls.o \
	adaptor-ioctl.o \

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/common/$(1)),)
unknown_drv += $(1)
else
common_drv += $(1)
total_subdrvs += $(1)
endif
else
platform_drv += $(1)
total_subdrvs += $(1)
endif
else
project_drv += $(1)
total_subdrvs += $(1)
endif
endef

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))

define add_subdrv
$(eval subdrv-rpath = $1)
$(eval include $(src)/$1/subdrv.mk)
endef

ifneq ($(unknown_drv),)
$(info unknown_drv: $(unknown_drv))
endif

ifneq ($(common_drv),)
$(info common_drv : $(common_drv))
$(foreach drv,$(common_drv),$(eval $(call add_subdrv,common/$(drv))))
endif

ifneq ($(platform_drv),)
$(info platform_drv : $(platform_drv))
$(foreach drv,$(platform_drv),$(eval $(call add_subdrv,$(MTK_PLATFORM)/$(drv))))
endif

ifneq ($(project_drv),)
$(info project_drv : $(project_drv))
$(foreach drv,$(project_drv),$(eval $(call add_subdrv,$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(drv))))
endif

comma := ,
empty :=
space := $(empty) $(empty)

IMGSENSOR_SUBDRVS := $(subst $(space),$(comma),$(foreach e,$(total_subdrvs),E($(e))))
subdir-ccflags-y += -D'IMGSENSOR_SUBDRVS=$(IMGSENSOR_SUBDRVS)' #-DHAVE_MTK_I2C_TRANSFER

$(info imgsensor-objs : $(imgsensor-objs))
$(info subdir-ccflags-y : $(subdir-ccflags-y))

obj-$(CONFIG_MTK_V4L2_IMGSENSOR) += imgsensor.o
