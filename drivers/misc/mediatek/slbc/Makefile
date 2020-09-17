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

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/sspm \
	-I$(srctree)/include \

ifeq ($(CONFIG_MTK_L3C_PART),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/l3c_part
endif

ifneq (strip $(CONFIG_MTK_SLBC),)
obj-${CONFIG_MTK_SLBC} += mtk_slbc.o
mtk_slbc-y += slbc.o
endif

ifneq ($(CONFIG_MTK_SLBC_MMSRAM),)
mtk_slbc-y += mmsram.o
endif

