obj-$(CONFIG_X86_INTEL_MID) += intel-mid.o intel_mid_vrtc.o mfld.o mrfl.o
obj-$(CONFIG_EARLY_PRINTK_INTEL_MID) += early_printk_intel_mid.o

obj-$(CONFIG_GMIN_INTEL_MID) += intel_mid_pcihelpers.o
obj-$(CONFIG_GMIN_INTEL_MID) += atomisp_gmin_platform.o

obj-y += device_libs/

# SFI specific code
ifdef CONFIG_X86_INTEL_MID
obj-$(CONFIG_SFI) += sfi.o
endif
