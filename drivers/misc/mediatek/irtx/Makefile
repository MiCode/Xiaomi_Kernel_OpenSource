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
ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761)))
obj-$(CONFIG_MTK_IRTX_PWM_SUPPORT) += $(subst ",,mt6765)/
else
obj-$(CONFIG_MTK_IRTX_PWM_SUPPORT) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

