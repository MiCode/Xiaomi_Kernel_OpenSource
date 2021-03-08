# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_cpas/include
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sensor_module/cam_sensor_io
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_sensor_module/cam_sensor_utils
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu
ccflags-y += -I$(srctree)

obj-$(CONFIG_SPECTRA_CAMERA) += cam_cci_dev.o cam_cci_core.o cam_cci_soc.o
