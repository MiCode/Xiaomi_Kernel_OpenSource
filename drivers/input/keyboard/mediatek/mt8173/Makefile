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

subdir-ccflags-y += -Werror -I$(srctree)/drivers/input/keyboard/mediatek/
ifeq ($(CONFIG_MTK_TC1_FM_AT_SUSPEND),y)
ifeq ($(CONFIG_MT_SND_SOC_V3),y)
	EXTRA_CFLAGS += -I$(srctree)/sound/soc/mediatek/mt_soc_audio_v3
endif
endif
obj-$(CONFIG_KEYBOARD_MTK) := hal_kpd.o
