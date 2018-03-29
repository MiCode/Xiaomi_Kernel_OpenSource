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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/power_gs/$(MTK_PLATFORM)/

obj-$(CONFIG_ARCH_MT6735) += mt_power_gs-v1.o
obj-$(CONFIG_ARCH_MT6735M) += mt_power_gs-v1.o
obj-$(CONFIG_ARCH_MT6753) += mt_power_gs-v1.o
obj-$(CONFIG_ARCH_MT6580) += mt_power_gs-v1.o
obj-$(CONFIG_ARCH_MT6755) += mt_power_gs-v1.o
obj-$(CONFIG_ARCH_MT6797) += mt_power_gs-v1.o

obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
