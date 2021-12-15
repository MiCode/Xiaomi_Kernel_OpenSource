# SPDX-License-Identifier: GPL-2.0

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

ifeq ($(CONFIG_MACH_MT6833),y)
obj-y += mt6833/
else ifeq ($(CONFIG_MACH_MT6781),y)
obj-y += mt6781/
else
obj-y += $(MTK_PLATFORM)/
endif
