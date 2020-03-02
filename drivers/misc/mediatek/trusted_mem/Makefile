# Copyright (C) 2018 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

# Modules Feature Options
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.configs

# Force strictly compiler check
ccflags-y += -Werror

#==========================================================================
# Trusted Memory Subsystem Driver
#==========================================================================
# Ensure module is built only when option is enabled
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) = trusted_mem.o

trusted_mem-y += dev_mgr.o
trusted_mem-y += entry.o
trusted_mem-y += region_mgr.o
trusted_mem-y += ssmr_mgr.o
trusted_mem-y += peer_mgr.o
trusted_mem-y += proc.o
trusted_mem-y += alloc_api.o

ifeq ($(TCORE_PROFILING_SUPPORT),y)
trusted_mem-y += profiler.o
ccflags-y += -DTCORE_PROFILING_SUPPORT
ifeq ($(TCORE_PROFILING_AUTO_DUMP),y)
ccflags-y += -DTCORE_PROFILING_AUTO_DUMP
endif # !TCORE_PROFILING_AUTO_DUMP
endif # !TCORE_PROFILING_SUPPORT

ifeq ($(TCORE_MEMORY_LEAK_DETECTION_SUPPORT),y)
trusted_mem-y += mld_helper.o
ccflags-y += -DTCORE_MEMORY_LEAK_DETECTION_SUPPORT
endif # !TCORE_KERN_MEMORY_LEAK_DETECTION_SUPPORT

ifeq ($(TCORE_UT_FWK_SUPPORT),y)
ccflags-y += -DTCORE_UT_FWK_SUPPORT
trusted_mem-y += ut_tests/core_ut.o
trusted_mem-y += ut_tests/ut_common.o
ifeq ($(TCORE_MEMORY_LEAK_DETECTION_SUPPORT),y)
trusted_mem-y += ut_tests/mld_ut.o
endif # !TCORE_KERN_MEMORY_LEAK_DETECTION_SUPPORT
ifeq ($(CONFIG_MTK_PROT_MEM_SUPPORT),y)
trusted_mem-y += ut_tests/pmem_ut.o
endif # !CONFIG_MTK_PROT_MEM_SUPPORT
ifeq ($(CONFIG_MTK_SECURE_MEM_SUPPORT),y)
trusted_mem-y += ut_tests/secmem_ut.o
endif # !CONFIG_MTK_SECURE_MEM_SUPPORT
ifeq ($(TCORE_PROFILING_SUPPORT),y)
trusted_mem-y += ut_tests/profiler_ut.o
endif # !TCORE_PROFILING_SUPPORT
ifeq ($(CONFIG_MTK_CAM_SECURITY_SUPPORT),y)
trusted_mem-y += ut_tests/fr_ut.o
endif # !CONFIG_MTK_CAM_SECURITY_SUPPORT
ifeq ($(CONFIG_MTK_WFD_SMEM_SUPPORT),y)
trusted_mem-y += ut_tests/wfd_smem_ut.o
endif # !CONFIG_MTK_WFD_SMEM_SUPPORT
ifeq ($(CONFIG_MTK_HAPP_MEM_SUPPORT),y)
trusted_mem-y += ut_tests/happ_ut.o
trusted_mem-y += ut_tests/happ_extra_ut.o
endif # !CONFIG_MTK_HAPP_MEM_SUPPORT
ifeq ($(CONFIG_MTK_SDSP_MEM_SUPPORT),y)
trusted_mem-y += ut_tests/sdsp_ut.o
endif # !CONFIG_MTK_SDSP_MEM_SUPPORT
ifeq ($(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT),y)
trusted_mem-y += ut_tests/sdsp_shared_ut.o
endif # !CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT
endif # !TCORE_UT_FWK_SUPPORT

#==========================================================================
# Protcted Shared Memory Core Driver
#==========================================================================
obj-$(CONFIG_MTK_PROT_MEM_SUPPORT) += pmem.o
pmem-y += pmem/pmem_device.o
pmem-y += pmem/pmem_api.o

ifeq ($(PMEM_MOCK_OBJECT_SUPPORT),y)
pmem-y += pmem/pmem_mock.o
pmem-y += pmem/memmgr_buddy.o
ccflags-y += -DPMEM_MOCK_OBJECT_SUPPORT
endif # !PMEM_MOCK_OBJECT_SUPPORT

#==========================================================================
# SVP Secure Memory Core Driver
#==========================================================================
obj-$(CONFIG_MTK_SECURE_MEM_SUPPORT) += secmem.o
secmem-y += secmem/secmem_device.o
secmem-y += secmem/secmem_api.o

ifeq ($(CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT),y)
obj-$(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE) += sdsp_smem.o
sdsp_smem-y += sdsp_smem/sdsp_smem_device.o
endif # !CONFIG_MTK_SDSP_SHARED_MEM_SUPPORT

#==========================================================================
# WFD Secure Memory Core Driver
#==========================================================================
obj-$(CONFIG_MTK_WFD_SMEM_SUPPORT) += wfd_smem.o
wfd_smem-y += wfd_smem/wfd_smem_device.o
wfd_smem-y += wfd_smem/wfd_smem_api.o

#==========================================================================
# MTEE Multiple Chunk Memory Core Driver
#==========================================================================
obj-$(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT) += mtee_chunks.o
ifeq ($(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT),y)
mtee_chunks-y += mtee_chunks/mtee_devices.o
endif

#==========================================================================
# Trustzone Peer Driver
#==========================================================================
ifneq ($(filter y,$(CONFIG_MTK_SECURE_MEM_SUPPORT) $(CONFIG_MTK_WFD_SMEM_SUPPORT) $(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE)),)
obj-y += tee_impl.o
tee_impl-y += tee_impl/tee_common.o
tee_impl-y += tee_impl/tee_invoke.o

ifneq ($(filter y,$(CONFIG_MTK_TEE_GP_SUPPORT) $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
tee_impl-y += tee_impl/tee_gp.o
endif
endif

#==========================================================================
# Geniezone Peer Driver
#==========================================================================
ifeq ($(CONFIG_MTK_GZ_KREE),y)
obj-y += mtee_impl.o
mtee_impl-y += mtee_impl/mtee_ops.o
mtee_impl-y += mtee_impl/mtee_invoke.o
endif

#==========================================================================
# Trusted Memory Subsystem Common Headers
#==========================================================================
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/public/
ifneq ($(wildcard $(srctree)/drivers/misc/mediatek/trusted_mem/$(MTK_PLATFORM)/),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/$(MTK_PLATFORM)/
else
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/common/
endif

#==========================================================================
# Other Modules Include Header Paths
#==========================================================================
ifeq ($(CONFIG_MTK_GZ_KREE),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/gz/
endif # !CONFIG_MTK_GZ_KREE

ifeq ($(CONFIG_MTK_SVP),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-ssvp/
endif # !CONFIG_MTK_SVP

ifeq ($(CONFIG_MTK_SSMR),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-ssmr/
endif # !CONFIG_MTK_SSMR

ifeq ($(CONFIG_MTK_ION),y)
ccflags-y += -I$(srctree)/drivers/staging/android/ion
ccflags-y += -I$(srctree)/drivers/staging/android/ion/mtk
endif # !CONFIG_MTK_ION

ifeq ($(CONFIG_MTK_TEE_GP_SUPPORT),y)
ccflags-y += -I$(srctree)/arch/arm/mach-$(MTK_PLATFORM)/include/trustzone/utils/tlutils
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/trustzone/utils/tlutils
ccflags-y += -I$(src)/$(MTK_PLATFORM)
endif # !CONFIG_MTK_TEE_GP_SUPPORT

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif # !CONFIG_TRUSTONIC_TEE_SUPPORT

ifeq ($(CONFIG_MICROTRUST_TEE_SUPPORT),y)
include $(srctree)/drivers/misc/mediatek/teei/Makefile.include
endif # !CONFIG_MICROTRUST_TEE_SUPPORT

ifeq ($(CONFIG_BLOWFISH_TEE_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/tzdev
endif # !CONFIG_BLOWFISH_TEE_SUPPORT
