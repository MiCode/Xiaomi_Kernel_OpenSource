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

PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ifeq (y, $(filter y, $(CONFIG_ARCH_MT6580) $(CONFIG_ARCH_MT6755) $(CONFIG_ARCH_MT6735) $(CONFIG_ARCH_MT6735M) $(CONFIG_ARCH_MT6753)))
subdir-ccflags-y += -DCOMMON_DISP_LOG
endif
ifeq ($(CONFIG_SINGLE_PANEL_OUTPUT),y)
    ifeq ($(CONFIG_ARCH_MT6735M), y)
    obj-y += common/
    obj-y += mt6735m/
    endif
else ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/ext_disp/$(PLATFORM)/),)
obj-y += $(PLATFORM)/
obj-y += common/
else
obj-y += dummy/
endif
