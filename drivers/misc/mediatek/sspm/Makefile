# SPDX-License-Identifier: GPL-2.0

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
else
obj-y +=  dummy.o
endif
