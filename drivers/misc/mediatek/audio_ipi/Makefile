#
# Copyright (C) 2016 MediaTek Inc.
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

ifeq ($(CONFIG_MACH_MT6761),y)
AUDIO_PLATFORM := mt6765
else ifeq ($(CONFIG_MACH_MT6781),y)
AUDIO_PLATFORM := mt6785
else
AUDIO_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
endif

ifneq ($(filter y,$(CONFIG_MTK_VOW_SUPPORT)),)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6785" "mt6768"))
CONFIG_MTK_VOW_IPI_SUPPORT:= y
endif
endif

ifneq ($(filter y,$(CONFIG_MTK_TINYSYS_SCP_SUPPORT)),)

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6785" "mt6781"))
ifneq ($(filter y, $(CONFIG_MTK_ULTRASND_PROXIMITY)),)
subdir-ccflags-y += -DCONFIG_MTK_AUDIO_CM4_SUPPORT
CONFIG_MTK_AUDIO_CM4_SUPPORT := y
endif
endif

ifneq ($(filter y,$(CONFIG_MTK_VOW_IPI_SUPPORT) $(CONFIG_SND_SOC_MTK_SCP_SMARTPA)),)
subdir-ccflags-y += -DCONFIG_MTK_AUDIO_CM4_SUPPORT
CONFIG_MTK_AUDIO_CM4_SUPPORT := y
endif

ifneq ($(filter y,$(CONFIG_SND_SOC_MTK_SCP_SMARTPA)),)
subdir-ccflags-y += -DCONFIG_MTK_AUDIO_CM4_DMA_SUPPORT
endif
endif

ifneq ($(filter y, $(CONFIG_MTK_AUDIO_CM4_SUPPORT) $(CONFIG_MTK_AUDIODSP_SUPPORT)),)
obj-y += $(subst ",,$(AUDIO_PLATFORM))/

ifeq ($(CONFIG_MACH_MT6779),y)
obj-y += common_v1/
else
obj-y += common_v2/
endif

else
# avoid build error
obj-y += audio_ipi_dummy.o
endif

