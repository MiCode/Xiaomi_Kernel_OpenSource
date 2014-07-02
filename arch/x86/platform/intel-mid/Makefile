obj-$(CONFIG_X86_INTEL_MID) += intel-mid.o intel_mid_vrtc.o mfld.o mrfl.o
obj-$(CONFIG_EARLY_PRINTK_INTEL_MID) += early_printk_intel_mid.o

obj-$(CONFIG_GMIN_INTEL_MID) += intel_mid_pcihelpers.o
obj-$(CONFIG_GMIN_INTEL_MID) += atomisp_gmin_platform.o

obj-$(subst m,y,$(CONFIG_VIDEO_MT9M114)) += device_libs/platform_mt9m114.o
obj-$(subst m,y,$(CONFIG_VIDEO_OV5693)) += device_libs/platform_ov5693.o
obj-$(subst m,y,$(CONFIG_VIDEO_OV2722)) += platform_ov2722.o

# SFI specific code
ifdef CONFIG_X86_INTEL_MID
obj-$(CONFIG_SFI) += sfi.o device_libs/
endif
