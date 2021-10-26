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
	-I$(srctree)/drivers/misc/mediatek/base/power/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/include \
	-I$(srctree)/drivers/misc/mediatek/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(CONFIG_MTK_PLATFORM)/include/mach \
	-I$(srctree)/drivers/misc/mediatek/l3c_part \
	-I$(srctree)/drivers/misc/mediatek/sspm \
	-I$(srctree)/drivers/misc/mediatek/sspm/$(CONFIG_MTK_PLATFORM) \
	-I$(srctree)/include \

MTK_SLBC_MODULE_LOCAL_NAME = mtk_slbc
MTK_SLBC_MODULE_SRC = slbc.o
ifeq ($(CONFIG_MTK_SLBC_MMSRAM), y)
MTK_SLBC_MODULE_SRC += mmsram.o
endif

$(MTK_SLBC_MODULE_LOCAL_NAME)-objs = $(MTK_SLBC_MODULE_SRC:.c=.o)
obj-${CONFIG_MTK_SLBC} += $(MTK_SLBC_MODULE_LOCAL_NAME).o
