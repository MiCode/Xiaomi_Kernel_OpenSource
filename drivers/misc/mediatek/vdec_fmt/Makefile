#
# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (C) 2016 MediaTek Inc.
#

# driver code for VDEC-FMT

ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmdvfs/ \
             -I$(srctree)/drivers/misc/mediatek/smi/ \
             -I$(srctree)/drivers/misc/mediatek/m4u/$(MTK_PLATFORM) \
             -I$(srctree)/drivers/iommu/ \
             -I$(srctree)/drivers/staging/android/mtk_ion/ \
             -I$(srctree)/drivers/staging/android/mtk_ion/mtk

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

