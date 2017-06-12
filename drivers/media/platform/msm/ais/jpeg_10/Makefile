GCC_VERSION      := $(shell $(CONFIG_SHELL) $(srctree)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)

ccflags-y += -Idrivers/media/platform/msm/ais/jpeg_10
ccflags-y += -Idrivers/media/platform/msm/ais/sensor/io
ccflags-y += -Idrivers/media/platform/msm/ais/common

obj-$(CONFIG_MSM_AIS_JPEG) += msm_jpeg_dev.o msm_jpeg_sync.o msm_jpeg_core.o msm_jpeg_hw.o msm_jpeg_platform.o
