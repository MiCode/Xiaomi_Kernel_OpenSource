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
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/battery
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/mediatek/charger
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek
subdir-ccflags-y += -Werror -I$(srctree)/drivers/power/supply/mediatek/misc
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

obj-$(CONFIG_TCPC_CLASS) += rt_pd_manager.o
