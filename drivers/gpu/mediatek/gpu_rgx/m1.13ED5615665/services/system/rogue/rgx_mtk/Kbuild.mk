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

PVRSRVKM_NAME = $(PVRSRV_MODNAME)

ccflags-y += \
	-Wno-error \

$(PVRSRVKM_NAME)-y += \
	services/system/rogue/common/env/linux/interrupt_support.o \
	services/system/rogue/common/env/linux/dma_support.o \
	services/system/rogue/common/vz_physheap_generic.o \
	services/system/rogue/common/vz_physheap_common.o \
	services/system/rogue/common/vmm_pvz_client.o \
	services/system/rogue/common/vmm_pvz_server.o \
	services/system/rogue/common/vz_vmm_pvz.o \
	services/system/rogue/common/vz_vmm_vm.o \
	services/system/rogue/common/vz_support.o \
	services/system/rogue/common/vmm_type_stub.o \
	services/system/rogue/$(PVR_SYSTEM)/ion_support.o \
	services/system/rogue/$(PVR_SYSTEM)/mtk_pp.o \
	services/system/rogue/$(PVR_SYSTEM)/sysconfig.o \
	services/system/rogue/$(PVR_SYSTEM)/$(MTK_PLATFORM)/mtk_mfgsys.o

ifeq ($(MTK_PLATFORM),mt6739)
$(PVRSRVKM_NAME)-y += \
	services/system/rogue/$(PVR_SYSTEM)/$(MTK_PLATFORM)/mtk_mfg_counter.o
endif

ccflags-y += \
	-I$(TOP)/services/system/rogue/$(PVR_SYSTEM) \
	-I$(TOP)/services/system/rogue/$(PVR_SYSTEM)/$(MTK_PLATFORM) \
	-I$(TOP)/services/system/rogue/common/env/linux \
	-I$(TOP)/services/linux/include \
	-I$(TOP)/kernel/drivers/staging/imgtec \
	-I$(srctree)/drivers/misc/mediatek/include/mt-plat \
	-I$(srctree)/drivers/staging/android/ion \
	-I$(srctree)/drivers/staging/android/ion/mtk \
	-I$(srctree)/drivers/gpu/mediatek \
	-I$(srctree)/drivers/gpu/mediatek/ged/include \
	-I$(srctree)/drivers/gpu/mediatek/mt-plat \
	-I$(srctree)/drivers/gpu/mediatek/gpu_bm \
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/include \
	-I$(srctree)/drivers/gpu/mediatek/gpufreq/$(MTK_PLATFORM) \
