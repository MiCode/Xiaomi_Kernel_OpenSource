# SPDX-License-Identifier: GPL-2.0 */
#
# Copyright (c) 2017 MediaTek Inc.
#

ifeq (y, $(filter y, $(CONFIG_MACH_MT6765) $(CONFIG_MACH_MT6761) $(CONFIG_MACH_MT6779)))

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/
obj-y += mtk_mdpm_common.o
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

else

ifeq ($(CONFIG_MACH_MT6833),y)
	PDIR = mt6833
else ifeq ($(CONFIG_MACH_MT6893),y)
	PDIR = mt6893
else ifeq ($(CONFIG_MACH_MT6877),y)
	PDIR = mt6877
else ifeq ($(CONFIG_MACH_MT6781),y)
	PDIR = mt6781
else
	PDIR = $(MTK_PLATFORM)
endif

ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/spm/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include/mdpm_v2/$(PDIR)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach/

obj-y += mtk_mdpm_common.o
obj-y += $(subst ",,$(PDIR))/

endif
