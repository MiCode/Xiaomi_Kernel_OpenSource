#
# Copyright (C) 2018 MediaTek Inc.
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

#DRAM_CONTROLLER

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6763"))
	ccflags-y += \
		-DVCORE_DVFS_OPP_SUPPORT
else
	ccflags-y += \
		-DMTK_QOS_SUPPORT

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6771"))
	ccflags-y += \
		-DMTK_QOS_EMI_OPP
endif # ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6771"))
endif # ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6739" "mt6763"))

obj-y += dram_ctrl.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/

#if PM_DEVFREQ
ccflags-y += -I$(srctree)/drivers/devfreq/
#endif # PM_DEVFREQ


