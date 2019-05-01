# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(srctree)/drivers/media/platform/msm/vidc/ \
    -I$(srctree)/drivers/devfreq/

msm-vidc-objs := msm_v4l2_vidc.o \
                msm_v4l2_private.o \
                msm_vidc_platform.o \
                msm_vidc_common.o \
                msm_vidc.o \
                msm_vdec.o \
                msm_venc.o \
                msm_cvp_internal.o \
                msm_cvp_external.o \
                msm_smem.o \
                msm_vidc_debug.o \
                msm_vidc_res_parse.o \
                hfi_common.o \
                hfi_ar50.o \
                hfi_iris1.o \
                hfi_iris2.o \
                hfi_response_handler.o \
                hfi_packetization.o \
                vidc_hfi.o \
                msm_vidc_clocks.o \
                msm_vidc_bus_iris1.o \
                msm_vidc_bus_iris2.o \
                msm_vidc_buffer_calculations.o

obj-$(CONFIG_MSM_VIDC_V4L2) := msm-vidc.o

