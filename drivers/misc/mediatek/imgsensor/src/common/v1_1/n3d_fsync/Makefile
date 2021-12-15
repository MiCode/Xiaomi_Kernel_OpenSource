# SPDX-License-Identifier: GPL-2.0

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

