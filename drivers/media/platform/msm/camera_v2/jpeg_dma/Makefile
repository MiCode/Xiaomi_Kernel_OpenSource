GCC_VERSION      := $(shell $(CONFIG_SHELL) $(srctree)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
ccflags-y += -Idrivers/media/video/msm
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
obj-$(CONFIG_MSM_JPEGDMA) += msm_jpeg_dma_dev.o msm_jpeg_dma_hw.o
