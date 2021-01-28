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

ifeq ($(CONFIG_MTK_PPM),y)
obj-y += cpu_ctrl.o
else
obj-y += cpu_ctrl_dummy.o
endif

obj-$(CONFIG_MTK_CPU_CTRL_CFP) += cpu_ctrl_cfp.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
