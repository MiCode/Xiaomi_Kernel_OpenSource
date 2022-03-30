# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

subdir-ccflags-y += \
	-I$(IMGSENSOR_DRIVER_PATH)/$(FRAME_SYNC) \

imgsensor-objs += \
	$(FRAME_SYNC)/frame_sync.o \
	$(FRAME_SYNC)/frame_sync_algo.o \
	$(FRAME_SYNC)/frame_monitor.o \
	$(FRAME_SYNC)/frame_sync_util.o \
	$(FRAME_SYNC)/frame_sync_console.o \
	$(FRAME_SYNC)/hw_sensor_sync_algo.o \
	$(FRAME_SYNC)/custom/custom_hw_sync.o \

