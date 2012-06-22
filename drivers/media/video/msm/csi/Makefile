GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
EXTRA_CFLAGS += -Idrivers/media/video/msm
obj-$(CONFIG_ARCH_MSM8960) += msm_csi2_register.o msm_csiphy.o msm_csid.o msm_ispif.o
obj-$(CONFIG_ARCH_MSM7X27A) += msm_csic_register.o msm_csic.o
obj-$(CONFIG_ARCH_MSM8X60) += msm_csic_register.o msm_csic.o
obj-$(CONFIG_ARCH_MSM7X30) += msm_csic_register.o msm_csic.o

