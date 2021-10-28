# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2021 MediaTek Inc.

MKFILE_PATH := $(lastword $(MAKEFILE_LIST))
IMGSENSOR_DRIVER_PATH := $(patsubst %/,%,$(dir $(realpath $(MKFILE_PATH))))
MTK_DRIVER_PATH := $(srctree)/drivers/misc/mediatek

subdir-ccflags-y += \
	-I$(MTK_DRIVER_PATH)/imgsensor/inc \
	-I$(IMGSENSOR_DRIVER_PATH)/ \

imgsensor-glue-objs := \
	imgsensor-glue.o \

$(info imgsensor-virt-sensor-objs : $(imgsensor-glue-objs))
$(info subdir-ccflags-y : $(subdir-ccflags-y))

obj-$(CONFIG_MTK_V4L2_IMGSENSOR) += imgsensor-glue.o
