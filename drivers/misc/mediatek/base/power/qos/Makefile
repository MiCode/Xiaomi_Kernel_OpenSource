#
# Copyright (C) 2018 MediaTek Inc.
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

ifdef CONFIG_MTK_PLATFORM
ifeq ($(CONFIG_MACH_MT6781),y)
MTK_PLATFORM := mt6781
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif
include $(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/Makefile
endif

ccflags-y += -I$(srctree)/include/memory/mediatek/
ifeq ($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/mt6785/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/dramc/$(MTK_PLATFORM)/
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/
ifneq ($(CONFIG_MTK_TINYSYS_SSPM_VERSION),"")
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_TINYSYS_SSPM_VERSION)
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/$(MTK_PLATFORM)/
endif

ifeq ($(SSPM_VER),v2)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v2
obj-y += mtk_qos_ipi_v2.o
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/sspm/v1
obj-y += mtk_qos_ipi.o
endif

ifeq ($(CONFIG_MACH_MT6877),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/mt6877
else ifeq($(CONFIG_MACH_MT6781),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(MTK_PLATFORM)/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/$(CONFIG_MTK_PLATFORM)
endif

obj-y += mtk_qos_main.o mtk_qos_bound.o mtk_qos_sram.o mtk_qos_sysfs.o
obj-$(QOS_PREFETCH_SUPPORT) += mtk_qos_prefetch.o
ccflags-$(QOS_PREFETCH_SUPPORT) += -DQOS_PREFETCH_SUPPORT

obj-$(QOS_SHARE_SUPPORT) += mtk_qos_share.o
ccflags-$(QOS_SHARE_SUPPORT) += -DQOS_SHARE_SUPPORT
# obj-y += $(MTK_PLATFORM)/

