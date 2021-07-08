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
PWM_BUILD_FOLDER=$(srctree)/drivers/misc/mediatek/pwm

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ifneq ($(filter y m,$(CONFIG_MTK_PWM)),)
ccflags-y += -I$(PWM_BUILD_FOLDER)/pwm_v2/include/
#mtk pwm_v2 source files
include $(PWM_BUILD_FOLDER)/pwm_v2/Makefile
endif # !CONFIG_MTK_PWM

MTK_PWM_V2 = $(mtk_pwm_v2_objs:%.o=pwm_v2/%.o)

#mtk pwm common source files
PWM_MODULE += mtk_pwm.o

#Generic build
ifneq ($(filter y m,$(CONFIG_MTK_PWM)),)
PWM_SRC = $(PWM_MODULE) $(MTK_PWM_V2)
obj-$(CONFIG_MTK_PWM)	= mtk-pwm.o
endif #!CONFIG_MTK_PWM

mtk-pwm-objs += $(PWM_SRC)
