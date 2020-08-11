# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sync
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cdm
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_lrme
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_lrme/lrme_hw_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_lrme/lrme_hw_mgr/lrme_hw
ccflags-y += -I$(srctree)/techpack/camera
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include

obj-$(CONFIG_SPECTRA_CAMERA) += lrme_hw/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_lrme_hw_mgr.o
