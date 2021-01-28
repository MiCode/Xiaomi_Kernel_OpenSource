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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdla/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdla/1.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/devfreq
ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vpu/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/

obj-$(CONFIG_MTK_MDLA_SUPPORT) := mdla_main.o gsm.o mdla_dvfs.o \
				mdla_debug.o mdla_trace.o mdla_decoder.o
obj-$(CONFIG_MTK_ION) += mdla_ion.o

