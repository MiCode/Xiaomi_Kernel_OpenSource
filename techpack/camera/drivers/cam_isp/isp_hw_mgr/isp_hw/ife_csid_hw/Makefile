# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/irq_controller
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr/

obj-$(CONFIG_SPECTRA_CAMERA) += cam_ife_csid_dev.o cam_ife_csid_soc.o cam_ife_csid_core.o
obj-$(CONFIG_SPECTRA_CAMERA) += cam_ife_csid17x.o cam_ife_csid_lite17x.o
