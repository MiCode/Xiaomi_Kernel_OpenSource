# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

MKFILE_PATH := $(lastword $(MAKEFILE_LIST))
IMGSENSOR_DRIVER_PATH := $(patsubst %/,%,$(dir $(realpath $(MKFILE_PATH))))
MTK_DRIVER_PATH := $(srctree)/drivers/misc/mediatek
MTK_ISP_DRIVER_PATH := $(srctree)/drivers/media/platform/mtk-isp
MTK_CAMSYS_DRIVER_PATH := $(srctree)/drivers/media/platform/mtk-isp/camsys/isp7_1/cam
IMGSENSOR_PROJECT := $(subst ",,$(CONFIG_MTK_IMGSENSOR_PROJECT))

FRAME_SYNC := frame-sync
VIRT_SENSOR := virt-sensor

subdir-ccflags-y += \
	-I$(MTK_DRIVER_PATH)/imgsensor/inc \
	-I$(IMGSENSOR_DRIVER_PATH)/ \
	-I$(MTK_ISP_DRIVER_PATH)/ \
	-I$(MTK_CAMSYS_DRIVER_PATH)/ \

imgsensor-objs := \
	adaptor-hw.o \
	adaptor-drv.o \
	adaptor-i2c.o \
	adaptor-ctrls.o \
	adaptor-ioctl.o \
	adaptor-common-ctrl.o \

define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/camera_project/$(IMGSENSOR_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/common/$(1)),)
unknown_drv += $(1)
else
common_drv += $(1)
total_subdrvs += $(1)
endif
else
project_drv += $(1)
total_subdrvs += $(1)
endif
ifneq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/camera_project/k6983v1_64_alpha/$(1)),)
alpha_drv += $(1)
total_subdrvs += $(1)
unknown_drv :=
endif
ifneq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/camera_project/k6895v1_64_95t_alpha/$(1)),)
alpha_drv_k6895 += $(1)
total_subdrvs += $(1)
unknown_drv :=
endif
endef

$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))

ifneq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(FRAME_SYNC)/frame_sync_drv.mk),)
$(info framesync_drv: $(FRAME_SYNC))
include $(IMGSENSOR_DRIVER_PATH)/$(FRAME_SYNC)/frame_sync_drv.mk
else
$(info framesync_drv: NOT FOUND)
endif

ifneq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(VIRT_SENSOR)/virt-sensor.mk),)
$(info virt-sensor: $(VIRT_SENSOR))
include $(IMGSENSOR_DRIVER_PATH)/$(VIRT_SENSOR)/virt-sensor.mk
else
$(info virt-sensor: NOT FOUND)
endif

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

ifneq ($(project_drv),)
$(info project_drv : $(project_drv))
$(foreach drv,$(project_drv),$(eval $(call add_subdrv,camera_project/$(IMGSENSOR_PROJECT)/$(drv))))
endif

ifneq ($(alpha_drv),)
$(info alpha_drv : $(alpha_drv))
$(foreach drv,$(alpha_drv),$(eval $(call add_subdrv,camera_project/k6983v1_64_alpha/$(drv))))
endif

ifneq ($(alpha_drv_k6895),)
$(info alpha_drv_k6895 : $(alpha_drv_k6895))
$(foreach drv,$(alpha_drv_k6895),$(eval $(call add_subdrv,camera_project/k6895v1_64_95t_alpha/$(drv))))
endif

comma := ,
empty :=
space := $(empty) $(empty)

IMGSENSOR_SUBDRVS := $(subst $(space),$(comma),$(foreach e,$(total_subdrvs),E($(e))))
ifneq ($(IMGSENSOR_SUBDRVS),)
subdir-ccflags-y += -D'IMGSENSOR_SUBDRVS=$(IMGSENSOR_SUBDRVS)'
endif

$(info imgsensor-objs : $(imgsensor-objs))
$(info subdir-ccflags-y : $(subdir-ccflags-y))

obj-$(CONFIG_MTK_V4L2_IMGSENSOR) += imgsensor-glue/
obj-$(CONFIG_MTK_V4L2_IMGSENSOR) += imgsensor.o
