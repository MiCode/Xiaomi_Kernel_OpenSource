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

ccflags-$(CONFIG_MTK_PSEUDO_M4U) += -I$(srctree)/drivers/misc/mediatek/pseudo_m4u/
ccflags-$(CONFIG_MTK_ION)  += -I$(srctree)/drivers/staging/android/mtk_ion #fro kernel-3.10 ion
ccflags-$(CONFIG_MTK_M4U) += -I$(srctree)/drivers/misc/mediatek/m4u/$(subst ",,$(CONFIG_MTK_PLATFORM))/

ccflags-$(CONFIG_MTK_M4U) += -I$(srctree)/drivers/misc/mediatek/m4u/2.0/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/mmp/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ifeq ($(CONFIG_MACH_MT6781), y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/mt6781/include
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(subst ",,$(CONFIG_MTK_PLATFORM))/include
endif
ccflags-y += -I$(srctree)/arch/arm/
ccflags-y += -I$(srctree)/drivers/iommu/

ccflags-y += -DION_HISTORY_RECORD

obj-$(CONFIG_MTK_ION) += ion_drv.o ion_mm_heap.o ion_fb_heap.o ion_sec_heap.o ion_profile.o
obj-$(CONFIG_MTK_ION) += ion_history.o
obj-$(CONFIG_MTK_ION) += ion_comm.o

ifeq ($(CONFIG_MTK_ENG_BUILD), y)
ccflags-y += -DION_RUNTIME_DEBUGGER=1
else
ccflags-y += -DION_RUNTIME_DEBUGGER=0
endif

ifeq ($(CONFIG_MTK_SECURE_MEM_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.include
endif

ifeq ($(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone
endif

ifeq ($(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM),y)
cflags-y += -I$(srctree)/drivers/misc/mediatek/secmem/includes
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.include
endif
