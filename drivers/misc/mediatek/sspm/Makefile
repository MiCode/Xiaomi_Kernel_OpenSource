ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
else
obj-y +=  dummy.o
endif
