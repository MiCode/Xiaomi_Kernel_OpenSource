#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
subdir-ccflags-y += -Wno-error
################################################################################
# FrameBuffer Driver for Display
#
#
################################################################################
ifneq ($(filter y,$(CONFIG_MACH_MT8127) $(CONFIG_MACH_MT7623) $(CONFIG_MACH_MT8590)),)
CONFIG_MTK_DISP_PLATFORM := mt8127
endif
ifeq ($(CONFIG_MTK_DISP_PLATFORM), "mt6779")
CONFIG_MACH_MT6779 := y
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include

ifneq ($(CONFIG_MTK_LCM), y)
	obj-y += mtdummy/
else
ifeq ($(CONFIG_MACH_MT6761),y)
	obj-$(CONFIG_MTK_FB) += mt6765/
else
	######################################
	obj-$(CONFIG_MTK_FB) += $(subst ",,$(CONFIG_MTK_DISP_PLATFORM))/
endif
	obj-$(CONFIG_MACH_MT6763) += common/
	obj-$(CONFIG_MACH_MT6755) += common/
	obj-$(CONFIG_MACH_MT6797) += common/
	obj-$(CONFIG_MACH_MT6757) += common/
	obj-$(CONFIG_MACH_KIBOPLUS) += common/
	obj-$(CONFIG_MACH_ELBRUS) += common/
	obj-$(CONFIG_MACH_MT8167) += common/
	obj-$(CONFIG_MACH_MT6799) += common/
	obj-$(CONFIG_MACH_MT6759) += common/
	obj-$(CONFIG_MACH_MT6758) += common/
	obj-$(CONFIG_MACH_MT6739) += common/
	obj-$(CONFIG_MACH_MT6765) += common/
	obj-$(CONFIG_MACH_MT6761) += common/
	obj-$(CONFIG_MACH_MT3967) += common/
	obj-$(CONFIG_MACH_MT6779) += common/
endif
