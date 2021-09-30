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
	-I$(srctree)/drivers/misc/mediatek/tinysys_scmi/ \
	-I$(srctree)/include \

ifeq ($(CONFIG_MTK_L3C_PART),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/l3c_part
endif

ifneq (strip $(CONFIG_MTK_SLBC),)
obj-${CONFIG_MTK_SLBC} += mtk_slbc.o
mtk_slbc-y += slbc.o
endif

ifneq ($(wildcard $(srctree)/$(src)/slbc_mt6893.c),)
obj-${CONFIG_MTK_SLBC_MT6893} += slbc_mt6893.o
endif

ifneq ($(wildcard $(srctree)/$(src)/slbc_mt6983.c),)
obj-${CONFIG_MTK_SLBC_MT6983} += slbc_mt6983.o
endif

ifneq ($(wildcard $(srctree)/$(src)/slbc_mt6895.c),)
obj-${CONFIG_MTK_SLBC_MT6895} += slbc_mt6895.o
endif

obj-${CONFIG_MTK_SLBC_IPI} += slbc_ipi.o
obj-${CONFIG_MTK_SLBC_MMSRAM} += mmsram.o

