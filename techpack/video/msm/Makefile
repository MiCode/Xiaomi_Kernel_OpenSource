# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -I$(srctree)/techpack/video/msm/vidc/ \
    -I$(srctree)/drivers/devfreq/

msm-vidc-objs := vidc/msm_v4l2_vidc.o \
                vidc/msm_vidc_platform.o \
                vidc/msm_vidc_common.o \
                vidc/msm_vidc.o \
                vidc/msm_vdec.o \
                vidc/msm_venc.o \
                vidc/msm_smem.o \
                vidc/msm_vidc_debug.o \
                vidc/msm_vidc_res_parse.o \
                vidc/hfi_common.o \
                vidc/hfi_ar50_lt.o \
                vidc/hfi_iris2.o \
                vidc/hfi_response_handler.o \
                vidc/hfi_packetization.o \
                vidc/vidc_hfi.o \
                vidc/msm_vidc_clocks.o \
                vidc/msm_vidc_bus_ar50lite.o\
                vidc/msm_vidc_bus_iris2.o \
                vidc/msm_vidc_buffer_calculations.o

obj-$(CONFIG_MSM_VIDC_V4L2) := msm-vidc.o

