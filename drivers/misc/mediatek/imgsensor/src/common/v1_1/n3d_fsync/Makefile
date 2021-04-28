#
# Copyright (C) 2021 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
#
# Makefile for misc devices that really don't fit anywhere else.
#
obj-y += n3d.o
obj-y += n3d_clk.o
obj-y += n3d_hw.o
obj-y += vsync_recorder.o

MKFILE_PATH := $(lastword $(MAKEFILE_LIST))
N3D_DRIVER_PATH := $(patsubst %/,%,$(dir $(realpath $(MKFILE_PATH))))
FRAME_SYNC := frame-sync

include $(N3D_DRIVER_PATH)/$(FRAME_SYNC)/frame_sync_drv.mk

