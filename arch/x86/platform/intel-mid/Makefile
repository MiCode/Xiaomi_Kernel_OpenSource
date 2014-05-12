obj-$(CONFIG_X86_INTEL_MID) += intel-mid.o intel_mid_vrtc.o mfld.o mrfl.o
obj-$(CONFIG_EARLY_PRINTK_INTEL_MID) += early_printk_intel_mid.o

obj-$(CONFIG_GMIN_INTEL_MID) += intel_mid_pcihelpers.o platform_imx134.o atomisp_gmin_platform.o
obj-$(CONFIG_GMIN_INTEL_MID) += platform_ov2722.o
obj-$(CONFIG_GMIN_INTEL_MID) += platform_byt_regulator.o

# SFI specific code
ifdef CONFIG_X86_INTEL_MID
obj-$(CONFIG_SFI) += sfi.o device_libs/
endif
