ifneq (,$(findstring 3gear,$(CONFIG_BUILD_ARM64_DTB_OVERLAY_IMAGE_NAMES)))
obj-y += mt6893/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
