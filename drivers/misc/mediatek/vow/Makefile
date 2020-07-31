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

ifeq ($(CONFIG_MACH_MT6893),y)
MTK_PLATFORM := mt6885
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

ifeq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/$(MTK_PLATFORM)
ifneq (,$(filter $(MTK_PLATFORM), "mt6785" "mt6771" "mt6768"))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
endif
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vow/$(MTK_PLATFORM)

# MTK vow driver interface
obj-y += $(subst ",,$(MTK_PLATFORM))/
obj-y += vow.o
