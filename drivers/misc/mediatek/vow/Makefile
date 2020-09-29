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

DSP_VERSION := $(subst ",,$(CONFIG_MTK_VOW_DSP_VERSION))

ccflags-y += -I$(srctree)/drivers/misc/mediatek/vow/$(DSP_VERSION)

obj-$(CONFIG_MTK_VOW_SUPPORT) += $(DSP_VERSION)/
