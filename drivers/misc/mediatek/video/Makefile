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

################################################################################
# FrameBuffer Driver for Display
#
#
################################################################################

ifneq ($(CONFIG_MTK_LCM), y)
	obj-y += mtdummy/
else
	######################################
# separate mt6833 from family mt6853
ifeq (y, $(filter y, $(CONFIG_MACH_MT6833)))
	obj-$(CONFIG_MTK_FB) += mt6833/
else
	obj-$(CONFIG_MTK_FB) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
	obj-$(CONFIG_MACH_MT6771) += common/
	obj-$(CONFIG_MACH_MT6768) += common/
	obj-$(CONFIG_MACH_MT8167) += common/
	obj-$(CONFIG_MACH_MT8168) += common/
	obj-$(CONFIG_MACH_MT6739) += common/
	obj-$(CONFIG_MACH_MT6785) += common/
	obj-$(CONFIG_MACH_MT6885) += common/
	obj-$(CONFIG_MACH_MT6765) += common/
	obj-$(CONFIG_MACH_MT6873) += common/
	obj-$(CONFIG_MACH_MT6853) += common/
	obj-$(CONFIG_MACH_MT6893) += common/
	obj-$(CONFIG_MACH_MT6833) += common/
endif

