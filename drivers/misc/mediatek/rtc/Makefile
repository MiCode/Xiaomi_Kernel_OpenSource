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

ifneq (,$(filter y,$(CONFIG_MTK_PMIC_CHIP_MT6358)))
  obj-$(CONFIG_MTK_RTC) += rtc-mt6358.o mt6358_misc.o
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/pmic/
ccflags-$(CONFIG_MTK_PMIC_CHIP_MT6358)+= -I$(srctree)/drivers/misc/mediatek/pmic/include/mt6358

ifneq (,$(filter y,$(CONFIG_MTK_PMIC_CHIP_MT6357)))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/rtc/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/rtc/mt6357
obj-$(CONFIG_MTK_RTC) += mtk_rtc_common.o mtk_rtc_hal_common.o
obj-$(CONFIG_MTK_RTC) += mt6357/
endif

obj-$(CONFIG_MTK_RTC) += mtk_rtc_debug.o
