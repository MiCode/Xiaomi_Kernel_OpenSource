#
# Copyright (C) 2016 MediaTek Inc.
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


ifeq ($(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT),y)

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/audio_ipi/common/utility \
	-I$(srctree)/drivers/misc/mediatek/audio_ipi/common/framework

obj-y += audio_ipi_client_spkprotect.o

ifeq ($(CONFIG_MT_ENG_BUILD),y)
CFLAGS_audio_ipi_client_spkprotect.o += -DDEBUG
endif

endif



