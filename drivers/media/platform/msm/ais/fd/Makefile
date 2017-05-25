GCC_VERSION      := $(shell $(CONFIG_SHELL) $(srctree)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
ccflags-y += -Idrivers/media/video/msm
ccflags-y += -Idrivers/media/platform/msm/ais/common
ccflags-y += -Idrivers/media/platform/msm/ais
ccflags-y += -Idrivers/media/platform/msm/ais/pproc/cpp
ccflags-y += -Idrivers/media/platform/msm/ais/msm_buf_mgr/

obj-$(CONFIG_MSM_AIS_FD) += msm_fd_dev.o msm_fd_hw.o
