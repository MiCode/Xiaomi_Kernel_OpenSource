GCC_VERSION      := $(shell $(CONFIG_SHELL) $(PWD)/scripts/gcc-version.sh $(CROSS_COMPILE)gcc)
ccflags-y += -Idrivers/media/video/msm
ifeq ($(CONFIG_MSM_CSI20_HEADER),y)
  ccflags-y += -Idrivers/media/video/msm/csi/include/csi2.0
else ifeq ($(CONFIG_MSM_CSI30_HEADER),y)
  ccflags-y += -Idrivers/media/video/msm/csi/include/csi3.0
endif
obj-$(CONFIG_MSM_CSI2_REGISTER) += msm_csi2_register.o
obj-$(CONFIG_MSM_CSIPHY) += msm_csiphy.o
obj-$(CONFIG_MSM_CSID) += msm_csid.o
obj-$(CONFIG_MSM_ISPIF) += msm_ispif.o
obj-$(CONFIG_ARCH_MSM8960) += msm_csi2_register.o msm_csiphy.o msm_csid.o msm_ispif.o
obj-$(CONFIG_ARCH_MSM7X27A) += msm_csic_register.o msm_csic.o
obj-$(CONFIG_ARCH_MSM8X60) += msm_csic_register.o msm_csic.o
obj-$(CONFIG_ARCH_MSM7X30) += msm_csic_register.o msm_csic.o
