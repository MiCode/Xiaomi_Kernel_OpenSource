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

ifeq ($(TCORE_UT_TESTS_SUPPORT),y)
ccflags-y += -DTCORE_UT_TESTS_SUPPORT
trusted_mem-y += tests/ut_common.o
trusted_mem-y += tests/ut_server.o
trusted_mem-y += tests/ut_cases.o
endif # !TCORE_UT_TESTS_SUPPORT

ifeq ($(CONFIG_MTK_GZ_KREE),y)
trusted_mem-y += mpu_vio_checker.o
endif

#==========================================================================
# TEE Seecure Memory Devices & Trustzone Peer Driver
#==========================================================================
ifneq ($(filter y,$(CONFIG_MTK_SECURE_MEM_SUPPORT) $(CONFIG_MTK_WFD_SMEM_SUPPORT) $(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE)),)
obj-y += tee_devices.o
obj-y += tee_impl.o
tee_impl-y += tee_impl/tee_invoke.o

ifneq ($(filter y,$(CONFIG_MTK_TEE_GP_SUPPORT) $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
tee_impl-y += tee_impl/tee_ops.o
endif
endif

#==========================================================================
# MTEE Multiple Chunk Devices & Geniezone Peer Driver
#==========================================================================
ifneq ($(filter y,$(CONFIG_MTK_PROT_MEM_SUPPORT) $(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)),)
obj-y += mtee_devices.o
endif
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

#==========================================================================
# Other Modules Include Header Paths
#==========================================================================
ifeq ($(CONFIG_MTK_GZ_KREE),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/gz/
endif # !CONFIG_MTK_GZ_KREE
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

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
