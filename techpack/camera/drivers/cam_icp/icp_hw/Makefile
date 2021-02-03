# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_icp
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_icp/icp_hw/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_icp/icp_hw/icp_hw_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include

obj-$(CONFIG_SPECTRA_CAMERA) += icp_hw_mgr/ a5_hw/ ipe_hw/ bps_hw/
