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

obj-y := emi_reg_rw.o

ifeq ($(CONFIG_MTK_COMMON_EMI), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt8127/include/
obj-y += mt8127/
else
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif