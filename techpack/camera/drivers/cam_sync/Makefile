# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_utils
ccflags-$(CONFIG_MSM_GLOBAL_SYNX) += -I$(srctree)/drivers/media/platform/msm/synx
ccflags-y += -I$(src)

obj-$(CONFIG_SPECTRA_CAMERA) += cam_sync.o cam_sync_util.o
