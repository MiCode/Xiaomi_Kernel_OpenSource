GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/video/msm
obj-$(CONFIG_ARCH_MSM8960) += msm_csiphy.o msm_csid.o msm_ispif.o
