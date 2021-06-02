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

subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/chip/

obj-$(CONFIG_MTK_CHIP) += CHIP_COMMON.o
CHIP_COMMON-$(CONFIG_MTK_CHIP) := mtk_chip_common.o

ifeq ($(CONFIG_MACH_MT6833),y)
obj-$(CONFIG_MTK_CHIP) += mt6833/
else
obj-$(CONFIG_MTK_CHIP) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif
