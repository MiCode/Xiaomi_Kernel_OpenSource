# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sync/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cdm/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_jpeg/jpeg_hw/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_jpeg/jpeg_hw/include/

obj-$(CONFIG_SPECTRA_CAMERA) += jpeg_enc_hw/
obj-$(CONFIG_SPECTRA_CAMERA) += jpeg_dma_hw/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_jpeg_hw_mgr.o
