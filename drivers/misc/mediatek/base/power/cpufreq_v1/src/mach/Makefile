ifeq ($(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES),"mediatek/mt6885_3gear")
obj-y += mt6893/
else
ifeq ($(CONFIG_MACH_MT6893),y)
obj-y += mt6893/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
endif
