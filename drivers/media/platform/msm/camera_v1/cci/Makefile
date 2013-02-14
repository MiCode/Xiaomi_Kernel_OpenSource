GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
ccflags-y += -Idrivers/media/platform/msm/camera_v1 -Idrivers/media/platform/msm/camera_v1/server
obj-$(CONFIG_MSM_CCI) += msm_cci.o
