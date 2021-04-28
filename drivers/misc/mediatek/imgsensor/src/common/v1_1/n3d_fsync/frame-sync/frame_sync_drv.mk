# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2020 MediaTek Inc.

subdir-ccflags-y += \
	-I$(N3D_DRIVER_PATH) \
	-I$(N3D_DRIVER_PATH)/$(FRAME_SYNC) \

obj-y += \
	$(FRAME_SYNC)/frame_sync.o \
	$(FRAME_SYNC)/frame_sync_algo.o \
	$(FRAME_SYNC)/frame_monitor.o \

