# SPDX-License-Identifier: GPL-2.0-only
ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common/
ifeq ($(CONFIG_MSM_ISP_V1),y)
obj-$(CONFIG_MSMB_CAMERA) += msm_isp_32.o msm_buf_mgr.o msm_isp_util_32.o msm_isp_axi_util_32.o msm_isp_stats_util_32.o
obj-$(CONFIG_MSMB_CAMERA) += msm_isp32.o
else
obj-$(CONFIG_MSMB_CAMERA) += msm_buf_mgr.o msm_isp_util.o msm_isp_axi_util.o msm_isp_stats_util.o
obj-$(CONFIG_MSMB_CAMERA) += msm_isp48.o msm_isp47.o msm_isp46.o msm_isp44.o msm_isp40.o msm_isp.o
endif
