#
# Copyright (C) 2018 MediaTek Inc.
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


ifeq ($(CONFIG_MTK_PPM),y)
obj-y += cpu_ctrl.o
else

# no CONFIG_MTK_PPM but specific lagacy platform

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6768" "mt6873" "mt6885" "mt6893" "mt6771" "mt6785" "mt6853" "mt6739" "mt8168" "mt8167"))
obj-y += cpu_ctrl.o
else
obj-y += cpu_ctrl_dummy.o

endif

endif # CONFIG_MTK_PPM

obj-$(CONFIG_MTK_CPU_CTRL_CFP) += cpu_ctrl_cfp.o
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
