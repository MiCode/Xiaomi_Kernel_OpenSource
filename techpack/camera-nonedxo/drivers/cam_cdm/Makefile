# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr

obj-$(CONFIG_SPECTRA_CAMERA) += cam_cdm_soc.o cam_cdm_util.o cam_cdm_intf.o\
				cam_cdm_core_common.o cam_cdm_virtual_core.o \
				cam_cdm_hw_core.o
