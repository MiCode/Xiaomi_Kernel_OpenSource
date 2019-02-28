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


ifeq ($(CONFIG_MTK_IRTX_SUPPORT), y)
obj-y := mt_irtx.o
else
ifeq ($(CONFIG_MTK_IRTX_PWM_SUPPORT), y)
obj-y := mtk_irtx_pwm.o
else
obj- := dummy.o
endif
endif

