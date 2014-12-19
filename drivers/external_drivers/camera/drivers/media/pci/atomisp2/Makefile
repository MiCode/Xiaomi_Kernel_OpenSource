ifdef CONFIG_GMIN_INTEL_MID
obj-$(CONFIG_VIDEO_ATOMISP) += css2400b0_v21_build/
obj-$(CONFIG_VIDEO_ATOMISP) += css2401a0_v21_build/
else
ifeq ($(CONFIG_64BIT), )
	obj-$(CONFIG_VIDEO_ATOMISP) += css2300_build/
endif
obj-$(CONFIG_VIDEO_ATOMISP) += css2400b0_v21_build/
obj-$(CONFIG_VIDEO_ATOMISP) += css2401a0_legacy_v21_build/
obj-$(CONFIG_VIDEO_ATOMISP) += css2401a0_v21_build/
endif
