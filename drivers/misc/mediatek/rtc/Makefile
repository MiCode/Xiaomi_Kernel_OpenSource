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



