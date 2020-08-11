# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/techpack/camera/include/uapi
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_core/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_req_mgr/
ccflags-y += -I$(srctree)/techpack/camera/drivers/cam_smmu/

obj-$(CONFIG_SPECTRA_CAMERA) += cam_soc_util.o cam_io_util.o cam_packet_util.o cam_debug_util.o cam_trace.o cam_common_util.o
obj-$(CONFIG_SPECTRA_CAMERA) += cam_cx_ipeak.o
