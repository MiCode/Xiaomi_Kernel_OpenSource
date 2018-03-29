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

MTK_PLATFORM:= $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/mrdump


ifdef CONFIG_MTK_ION
subdir-ccflags-y += -I$(srctree)/drivers/staging/android/ion
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
endif

ifdef CONFIG_MTK_GPU_SUPPORT
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
endif


obj-y	+= common/
obj-$(CONFIG_MTK_AEE_FEATURE)	+= aed/
obj-$(CONFIG_MTK_AEE_FEATURE)	+= ipanic/
obj-y	+= mrdump/

