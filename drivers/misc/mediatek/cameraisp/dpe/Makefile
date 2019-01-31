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

obj-y := cameradpe_dummy.o

ifeq ($(CONFIG_MACH_MT6771),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_DPE_SUPPORT) += isp_50/camera_dpe.o
else ifeq ($(CONFIG_MACH_MT6775),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_DPE_SUPPORT) += isp_50/camera_dpe.o
else ifeq ($(CONFIG_MACH_MT6762),y)
  obj-$(CONFIG_MTK_CAMERA_ISP_DPE_SUPPORT) += mt6765/camera_dpe.o
else
  obj-$(CONFIG_MTK_CAMERA_ISP_DPE_SUPPORT) += $(MTK_PLATFORM)/camera_dpe.o
endif
