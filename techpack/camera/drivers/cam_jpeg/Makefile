# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sync
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_jpeg/jpeg_hw/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_jpeg/jpeg_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include/

obj-$(CONFIG_SPECTRA_CAMERA) += jpeg_hw/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_jpeg_dev.o cam_jpeg_context.o
