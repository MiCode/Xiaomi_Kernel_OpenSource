# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/irq_controller
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/ife_csid_hw
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cust/cam_custom_hw_mgr/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cust/cam_custom_hw_mgr/cam_custom_hw1/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sync
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include

obj-$(CONFIG_SPECTRA_CAMERA) += cam_custom_hw1/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_custom_hw1/ cam_custom_csid/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_custom_hw_mgr.o

