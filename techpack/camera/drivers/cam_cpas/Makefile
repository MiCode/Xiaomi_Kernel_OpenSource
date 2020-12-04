# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include
#ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/cpas_top
#ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/camss_top
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)

obj-$(CONFIG_SPECTRA_CAMERA) += cpas_top/
obj-$(CONFIG_SPECTRA_CAMERA) += camss_top/
obj-$(CONFIG_SPECTRA_CAMERA) += cam_cpas_soc.o cam_cpas_intf.o cam_cpas_hw.o