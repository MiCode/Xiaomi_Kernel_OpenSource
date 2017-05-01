ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common/
ifeq ($(CONFIG_MSM_ISP_V1),y)
obj-$(CONFIG_MSM_CSID) += msm_ispif_32.o
else
obj-$(CONFIG_MSM_CSID) += msm_ispif.o
endif