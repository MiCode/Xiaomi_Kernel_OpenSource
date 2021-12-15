# SPDX-License-Identifier: GPL-2.0

#
# Makefile for misc devices that really don't fit anywhere else.
#
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
IMGSENSOR_DRIVER_PATH  := $(srctree)/drivers/misc/mediatek/imgsensor/src
MTK_DRIVER_PATH  := $(srctree)/drivers/misc/mediatek
COMMON_VERSION := v1_1
subdir-ccflags-y += -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION) \
	-I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/seninf/ \
	-I$(MTK_DRIVER_PATH)/mmdvfs \
	-I$(srctree)/drivers/i2c/busses/

ifeq ($(CONFIG_MTK_PLATFORM), "mt6885")
subdir-ccflags-y += -DIMGSENSOR_MT6885
endif

obj-y += ../common/$(COMMON_VERSION)/imgsensor.o
obj-y += ../common/$(COMMON_VERSION)/imgsensor_hw.o
obj-y += ../common/$(COMMON_VERSION)/imgsensor_i2c.o
obj-y += ../common/$(COMMON_VERSION)/imgsensor_legacy.o
obj-y += ../common/$(COMMON_VERSION)/imgsensor_proc.o
obj-y += ../common/$(COMMON_VERSION)/imgsensor_sensor_list.o
obj-y += ../common/$(COMMON_VERSION)/seninf_clk.o
obj-y += ../common/$(COMMON_VERSION)/seninf.o
obj-y += ../common/$(COMMON_VERSION)/imgsensor_pwr_seq.o
define FILTER_DRV
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/$(1)),)
ifeq ($(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/$(1)),)
common_drv += ../common/$(COMMON_VERSION)/$(1)/
else
platform_drv += $(1)/
endif
else
project_drv += ./camera_project/$(ARCH_MTK_PROJECT)/$(1)/
endif
endef
$(foreach drv,$(subst $\",,$(CONFIG_CUSTOM_KERNEL_IMGSENSOR)),$(eval $(call FILTER_DRV,$(drv))))
ifneq ($(common_drv),)
$(info imgsensor drv by common $(common_drv))
endif
obj-y				+= $(common_drv)
ifneq ($(platform_drv),)
$(info imgsensor drv by platform $(platform_drv))
endif

$(info imgsensor drv by platform $(project_drv))

obj-y				+= $(platform_drv)
ifeq (,$(wildcard $(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw))
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
obj-y				+= camera_hw/
ifneq ($(project_drv),)
obj-y				+= camera_project/$(ARCH_MTK_PROJECT)/
endif
else
obj-y				+= camera_project/$(ARCH_MTK_PROJECT)/
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_project/$(ARCH_MTK_PROJECT)/camera_hw
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/$(MTK_PLATFORM)/camera_hw
endif
subdir-ccflags-y		+= -I$(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/camera_hw

# Enable parallelism
subdir-ccflags-y += -DNO_I2C_MTK -DSENSOR_PARALLEISM

obj-y += seninf/
