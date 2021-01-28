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

ifeq (y, $(filter y, $(CONFIG_MACH_MT6761)))
MTK_KPD_PLATFORM:=mt6765
else
MTK_KPD_PLATFORM:=$(CONFIG_MTK_PLATFORM)
endif
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_KPD_PLATFORM)/include/
subdir-ccflags-y += -I$(srctree)/drivers/input/keyboard/mediatek/$(MTK_KPD_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
obj-$(CONFIG_KEYBOARD_MTK) := kpd.o
obj-y += $(subst ",,$(MTK_KPD_PLATFORM))/

