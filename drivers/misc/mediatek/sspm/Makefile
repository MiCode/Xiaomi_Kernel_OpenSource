ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
obj-y += $(subst ",,$(CONFIG_MTK_TINYSYS_SSPM_VERSION))/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
else
obj-y +=  dummy.o
endif
