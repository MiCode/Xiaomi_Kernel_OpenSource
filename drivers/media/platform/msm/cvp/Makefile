# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(srctree)/drivers/media/platform/msm/cvp/
ccflags-y += -I$(srctree)/drivers/media/platform/msm/synx/

msm-cvp-objs := cvp.o \
				msm_cvp_ioctl.o \
				msm_cvp_platform.o \
				msm_cvp_common.o \
				msm_cvp_core.o \
				msm_cvp.o \
				msm_smem.o \
				msm_cvp_debug.o \
				msm_cvp_res_parse.o \
				cvp_hfi.o \
				hfi_response_handler.o \
				hfi_packetization.o \
				cvp_core_hfi.o \
				msm_cvp_clocks.o \
				msm_cvp_dsp.o \
				msm_cvp_buf.o \
				msm_cvp_synx.o \
				cvp_fw_load.o

obj-$(CONFIG_MSM_CVP) := msm-cvp.o
