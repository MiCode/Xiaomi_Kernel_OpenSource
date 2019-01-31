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

ifneq ($(filter y,$(CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT) $(CONFIG_MTK_AUDIO_TUNNELING_SUPPORT) $(CONFIG_MTK_VOW_SUPPORT) $(CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT)),)
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-y += common/
else
# avoid build error
obj-y += audio_ipi_dummy.o
endif

