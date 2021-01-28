# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2019 MediaTek Inc.
#

ccflags-y += -I$(srctree)/drivers/staging/android/mtk_ion
ccflags-y += -I$(srctree)/drivers/misc/mediatek/emi/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/m4u/$(CONFIG_MTK_PLATFORM)/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/smi/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/vpu/3.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/devfreq
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mdla/1.0
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/qos/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/qos/
ifeq ($(CONFIG_MTK_IOMMU_V2),y)
ccflags-y += -I$(srctree)/drivers/iommu
endif

obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu_hw.o vpu_reg.o apu_dvfs.o apu_dvfs_sysfs.o
obj-$(CONFIG_MTK_VPU_SUPPORT) += vpu_profile.o vpu_qos.o vpu_dump.o
