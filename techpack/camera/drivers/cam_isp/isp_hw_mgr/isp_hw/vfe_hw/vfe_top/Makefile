# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cdm/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/irq_controller
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw

obj-$(CONFIG_SPECTRA_CAMERA) += cam_vfe_camif_lite_ver2.o cam_vfe_top.o cam_vfe_top_common.o
obj-$(CONFIG_SPECTRA_CAMERA) += cam_vfe_top_ver3.o cam_vfe_top_ver2.o cam_vfe_camif_ver2.o
obj-$(CONFIG_SPECTRA_CAMERA) += cam_vfe_camif_ver3.o cam_vfe_rdi.o cam_vfe_fe_ver1.o cam_vfe_camif_lite_ver3.o
