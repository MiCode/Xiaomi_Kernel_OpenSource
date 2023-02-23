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
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/mt6765/include
obj-y += $(subst ",,mt6765)/
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6781)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/mt6781/include
obj-y += $(subst ",,mt6781)/
else ifeq (y, $(filter y, $(CONFIG_MACH_MT6877)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/mt6877/include
obj-y += $(subst ",,mt6877)/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/pwm/$(CONFIG_MTK_PLATFORM)/include
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/
endif

obj-$(CONFIG_MTK_PWM)	+= mtk_pwm.o


