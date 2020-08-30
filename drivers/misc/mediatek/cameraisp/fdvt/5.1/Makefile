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

#
# Makefile for misc devices that really don't fit anywhere else.
#

#ifdef CONFIG_MTK_M4U
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
#else
ccflags-y += -I$(srctree)/drivers/iommu/
#endif
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/mailbox/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/cmdq/v3/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/staging/android/ion \
						 -I$(srctree)/drivers/staging/android/ion/mtk

ifeq ($(strip $(CONFIG_MTK_CAM_GENIEZONE_SUPPORT)),y)
ifneq (,$(filter $(CONFIG_MTK_PLATFORM), "mt6885" "mt6873" "mt6853"))
ccflags-y += -DCMDQ_MTEE
endif
endif

obj-$(CONFIG_MTK_CAMERA_ISP_FD_SUPPORT) += camera_fdvt.o
