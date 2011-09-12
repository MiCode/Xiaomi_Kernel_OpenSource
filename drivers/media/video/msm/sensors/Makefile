GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/video/msm
EXTRA_CFLAGS += -Idrivers/media/video/msm/io
obj-$(CONFIG_MSM_CAMERA_SENSOR) += msm_sensor.o
obj-$(CONFIG_IMX074) += imx074_v4l2.o
obj-$(CONFIG_OV2720) += ov2720.o
