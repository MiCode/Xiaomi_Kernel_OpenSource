# SPDX-License-Identifier: GPL-2.0

ifneq ($(strip $(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)$(CONFIG_MTK_TINYSYS_SSPM_PLT_SUPPORT)),)
ifeq ($(CONFIG_MTK_TINYSYS_SSPM_V2),y)
obj-$(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) += v2/
endif
else
obj-y +=  dummy.o
endif
