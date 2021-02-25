# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/include/
ccflags-y += -I$(srctree)/

obj-$(CONFIG_SPECTRA_CAMERA) += cam_req_mgr_core.o\
				cam_req_mgr_dev.o \
				cam_req_mgr_util.o \
				cam_req_mgr_workq.o \
				cam_mem_mgr.o \
				cam_req_mgr_timer.o \
				cam_req_mgr_debug.o
