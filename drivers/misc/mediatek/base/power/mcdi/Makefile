#
# Copyright (C) 2017 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/$(CONFIG_MTK_PLATFORM)/

obj-y += mtk_mcdi_governor.o
obj-y += mtk_mcdi_governor_lib.o
obj-y += mtk_mcdi_governor_hint.o
obj-y += mtk_mcdi_api.o
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

ifeq ($(CONFIG_MACH_MT6771), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/mcdi_v1/
obj-y += mcdi_v1/
endif

ifeq ($(CONFIG_MACH_MT6739), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/mcdi_v1/
obj-y += mcdi_v1/
endif

ifeq ($(CONFIG_MACH_MT6768), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/mcdi_v1/
obj-y += mcdi_v1/
endif

ifeq ($(CONFIG_MACH_MT6765), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/mcdi_v1/
obj-y += mcdi_v1/
endif

ifeq ($(CONFIG_MACH_MT6785), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/mcdi_v1/
obj-y += mcdi_v1/
endif