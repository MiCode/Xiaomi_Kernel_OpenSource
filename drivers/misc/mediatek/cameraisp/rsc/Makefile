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

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))

$(info $(srctree))
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
             -I$(srctree)/drivers/staging/android/ion/mtk
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/

obj-y += camera_rsc_dummy.o
obj-y += engine_request.o

ifeq ($(CONFIG_MACH_MT6758),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += $(MTK_PLATFORM)/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6771),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_50/camera_rsc.o
endif

ifeq ($(CONFIG_MACH_MT6775),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += isp_50/camera_rsc.o

endif
ifeq ($(CONFIG_MACH_MT6779),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_RSC_SUPPORT) += mt6779/camera_rsc.o
endif
