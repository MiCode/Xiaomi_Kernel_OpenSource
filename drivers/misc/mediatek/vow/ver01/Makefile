#
# Copyright (C) 2020 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

ifeq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/scp/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework
endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vow

mtk-vow-objs := \
	vow.o \
	vow_scp.o
obj-$(CONFIG_MTK_VOW_SUPPORT) += mtk-vow.o
