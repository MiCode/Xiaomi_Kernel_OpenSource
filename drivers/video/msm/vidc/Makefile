ifdef CONFIG_MSM_VIDC_720P
EXTRA_CFLAGS += -Idrivers/video/msm/vidc/720p/ddl
EXTRA_CFLAGS += -Idrivers/video/msm/vidc/720p/resource_tracker
endif

ifdef CONFIG_MSM_VIDC_1080P
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/1080p/ddl
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/1080p/resource_tracker
endif

EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/dec
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/enc
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/vcd
EXTRA_CFLAGS  += -Idrivers/video/msm/vidc/common/init

obj-$(CONFIG_MSM_VIDC) += vidc.o


obj-$(CONFIG_MSM_VIDC_VDEC) += vidc_vdec.o


obj-$(CONFIG_MSM_VIDC_VENC) += vidc_venc.o

