#
# Makefile for sensor drivers
#

obj-$(CONFIG_VIDEO_IMX)        += imx/
obj-$(CONFIG_VIDEO_IMXVB)      += imx135VB/
obj-$(CONFIG_VIDEO_OV5693)     += ov5693.o

obj-$(CONFIG_VIDEO_MT9M114)    += mt9m114.o
obj-$(CONFIG_VIDEO_MT9E013)    += mt9e013/

obj-$(CONFIG_VIDEO_MT9D113)    += mt9d113.o

obj-$(CONFIG_VIDEO_MT9V113)    += mt9v113.o

obj-$(CONFIG_VIDEO_OV5640)     += ov5640.o

obj-$(CONFIG_VIDEO_GC0339)     += gc0339.o
obj-$(CONFIG_VIDEO_GC2235)     += gc2235.o
obj-$(CONFIG_VIDEO_OV9724)     += ov9724.o
obj-$(CONFIG_VIDEO_OV2722)     += ov2722.o
obj-$(CONFIG_VIDEO_OV8830)     += ov8830.o
obj-$(CONFIG_VIDEO_OV8858)     += ov8858_driver.o
ov8858_driver-objs := ov8858.o imx/dw9718.o imx/vcm.o

obj-$(CONFIG_VIDEO_S5K8AAY)    += s5k8aay.o
obj-$(CONFIG_VIDEO_S5K6B2YX)    += s5k6b2yx.o

m10mo_isp-objs := m10mo.o m10mo_fw.o m10mo_tables.o
obj-$(CONFIG_VIDEO_M10MO)    += m10mo_isp.o
obj-$(CONFIG_VIDEO_M10MO)    += m10mo_spi.o

obj-$(CONFIG_VIDEO_MSRLIST_HELPER) += libmsrlisthelper.o

obj-$(CONFIG_VIDEO_AP1302)     += ap1302.o
obj-$(CONFIG_VIDEO_OV680)     += ov680.o

obj-$(CONFIG_VIDEO_CSI_XACTOR) += xactor_x.o
obj-$(CONFIG_VIDEO_PIXTER) += pixter.o

#
# Makefile for flash drivers
#

obj-$(CONFIG_VIDEO_LM3554) += lm3554.o
obj-$(CONFIG_VIDEO_LM3559) += lm3559.o

ccflags-y += -Werror
