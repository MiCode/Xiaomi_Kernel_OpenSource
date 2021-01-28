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

ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
#ccflags-y += -I$(srctree)/drivers/staging/android/ion \
#             -I$(srctree)/drivers/staging/android/ion/mtk

ifeq ($(CONFIG_MTK_ION),y)
    ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion/
    ccflags-y  += -I$(srctree)/drivers/staging/android/mtk_ion/mtk/
else
     ccflags-$(CONFIG_ION) += -I$(srctree)/drivers/staging/android/aosp_ion/
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/$(MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/timer/timesync/

obj-y += camera_isp.o
obj-y += cam_qos.o