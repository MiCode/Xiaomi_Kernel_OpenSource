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

LOCAL_PATH := $(srctree)/drivers/misc/mediatek/devapc
PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

ifeq ($(TARGET_BUILD_VARIANT), eng)
subdir-ccflags-y += -DDBG_ENABLE
endif

$(info PLATFORM = $(PLATFORM))

ifneq ($(wildcard $(LOCAL_PATH)/$(PLATFORM)/Makefile),)
   $(info DEVAPC platform code exists...)
    obj-y += $(PLATFORM)/
else
   $(info DEVAPC platform code not exists...)
endif
