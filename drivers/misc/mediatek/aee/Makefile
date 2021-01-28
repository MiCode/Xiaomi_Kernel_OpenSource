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

ifeq ($(CONFIG_MTK_PLATFORM), "mt6761")
MTK_PLATFORM := mt6765
else ifneq ($(CONFIG_MTK_PLATFORM), "")
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
else
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_DISP_PLATFORM))
endif
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mrdump
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

subdir-ccflags-y += -I$(srctree)/drivers/watchdog/mediatek/include/

ifdef CONFIG_MTK_ION
subdir-ccflags-y += -I$(srctree)/drivers/staging/android/ion
endif

subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/

obj-y	+= common/
obj-y	+= aed/
obj-y	+= mboot_params/
obj-y	+= mrdump/
