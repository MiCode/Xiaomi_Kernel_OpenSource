#
# Copyright (C) 2021 MediaTek Inc.
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

# driver code for VDEC-FMT

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/iommu/ \
             -I$(srctree)/drivers/staging/android/ion/ \
             -I$(srctree)/drivers/staging/android/ion/mtk

ifeq ($(CONFIG_MACH_MT6781),y)
FMT_PLATFORM := mt6781
endif

obj-y += vdec_fmt_driver.o
obj-y += $(FMT_PLATFORM)/vdec_fmt_pm.o
obj-y += vdec_fmt_ion.o
obj-y += vdec_fmt_sync.o

ccflags-y +=  -I$(srctree)/drivers/misc/mediatek/vdec_fmt/
ccflags-y +=  -I$(srctree)/drivers/misc/mediatek/vdec_fmt/$(FMT_PLATFORM)
ccflags-y +=  -I$(srctree)/drivers/misc/mediatek/smi/$(FMT_PLATFORM)/

