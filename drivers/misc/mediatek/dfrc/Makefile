#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

obj-y += dfrc_driver.o

dfrc_driver-objs := \
	dfrc.o

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox \
	-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys

ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/video/include/     \
	-I$(srctree)/drivers/misc/mediatek/video/common/   \
	-I$(srctree)/drivers/misc/mediatek/video/common/rdma10/ \
	-I$(srctree)/drivers/misc/mediatek/video/common/wdma10/ \
	-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/dispsys/   \
	-I$(srctree)/drivers/misc/mediatek/video/$(MTK_PLATFORM)/videox/   \
	-I$(srctree)/drivers/misc/mediatek/sync/      \
	-I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/include/mach/ \
	-I$(srctree)/drivers/misc/mediatek/base/power/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/base/power/include \
	-I$(srctree)/drivers/misc/mediatek/gpu/ged/include

ccflags-$(CONFIG_MTK_LCM) += \
	-I$(srctree)/drivers/misc/mediatek/lcm/inc/

ccflags-y += \
	-I$(srctree)/drivers/staging/android/ion \
	-I$(srctree)/drivers/staging/android/ion/mtk

subdir-ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
	-I$(srctree)/drivers/misc/mediatek/m4u/2.0

subdir-ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/cmdq/v3/

ifeq ($(CONFIG_MACH_MT6761),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/mt6765/
else
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
endif

subdir-ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/mmp/

subdir-ccflags-$(CONFIG_MTK_LCM) += \
	-I$(srctree)/drivers/misc/mediatek/lcm/inc

ifeq ($(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT),y)
ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
ccflags-y += \
	-I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/public \
	-I$(srctree)/drivers/misc/mediatek/gud/$(MTK_PLATFORM)/gud/MobiCoreKernelApi/include \
	-I$(srctree)/../vendor/trustonic/platform/$(MTK_PLATFORM)/trustlets/tplay/Tltplay/public
endif
endif

ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6799"))
ccflags-y += \
	-DPLATFORM_SUPPORT_ARR
endif
