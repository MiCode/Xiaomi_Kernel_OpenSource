GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/platform/msm/camera_v1
obj-$(CONFIG_MSM_MERCURY) += msm_mercury_dev.o msm_mercury_core.o msm_mercury_hw.o msm_mercury_platform.o msm_mercury_sync.o
