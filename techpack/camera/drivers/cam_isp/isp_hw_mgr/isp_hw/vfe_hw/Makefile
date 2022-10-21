# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/hw_utils/irq_controller
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_bus/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_isp/isp_hw_mgr/isp_hw/vfe_hw/vfe_top/include

obj-$(CONFIG_SPECTRA_CAMERA) += cam_vfe_soc.o cam_vfe_dev.o cam_vfe_core.o
obj-$(CONFIG_SPECTRA_CAMERA) += vfe_bus/ vfe_top/ vfe17x/
