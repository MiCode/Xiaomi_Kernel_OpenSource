# SPDX-License-Identifier: GPL-2.0
#
# Copyright (C) 2018 MediaTek Inc.
#

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif

# Modules Feature Options
include $(srctree)/drivers/misc/mediatek/trusted_mem/Makefile.configs

# Force strictly compiler check
ccflags-y += -Werror

#==========================================================================
# Trusted Memory Subsystem Driver
#==========================================================================
# Ensure module is built only when option is enabled
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM) = trusted_mem.o
trusted_mem-y += ssmr/memory_ssmr.o
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

ifneq ($(filter y m,$(CONFIG_MTK_GZ_KREE)),)
trusted_mem-y += mpu_vio_checker.o
endif

#==========================================================================
# TEE Seecure Memory Devices & Trustzone Peer Driver
#==========================================================================
ifneq ($(filter y m,$(CONFIG_MTK_SECURE_MEM_SUPPORT) \
		  $(CONFIG_MTK_WFD_SMEM_SUPPORT) \
		  $(CONFIG_MTK_SDSP_SHARED_PERM_VPU_TEE)),)
trusted_mem-y += tee_devices.o
ccflags-y += -DTEE_DEVICES_SUPPORT
trusted_mem-y += tee_impl/tee_invoke.o

ifneq ($(filter y m,$(CONFIG_MICROTRUST_TEE_SUPPORT) \
		  $(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
trusted_mem-y += tee_impl/tee_ops.o
endif
endif

#==========================================================================
# MTEE Multiple Chunk Devices & Geniezone Peer Driver
#==========================================================================
ifneq ($(filter y m,$(CONFIG_MTK_PROT_MEM_SUPPORT) \
		  $(CONFIG_MTK_MTEE_MULTI_CHUNK_SUPPORT)),)
trusted_mem-y += mtee_devices.o
ccflags-y += -DMTEE_DEVICES_SUPPORT
endif
ifneq ($(filter y m,$(CONFIG_MTK_GZ_KREE)),)
trusted_mem-y += mtee_impl/mtee_ops.o
trusted_mem-y += mtee_impl/mtee_invoke.o
endif

#==========================================================================
# Trusted Memory Subsystem Common Headers
#==========================================================================
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/public/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/trusted_mem/ssmr
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

#==========================================================================
# Other Modules Include Header Paths
#==========================================================================
ifneq ($(filter y m,$(CONFIG_MTK_GZ_KREE)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/geniezone/public
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/gz/
endif # !CONFIG_MTK_GZ_KREE

ifneq ($(filter y m,$(CONFIG_MTK_SVP)),)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/memory-ssvp/
endif # !CONFIG_MTK_SVP

ifneq ($(filter y m,$(CONFIG_TRUSTONIC_TEE_SUPPORT)),)
ccflags-y += -I$(srctree)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public
ccflags-y += -I$(srctree)/drivers/tee/gud/$(CONFIG_TRUSTONIC_TEE_VERSION)/MobiCoreDriver/public/GP
endif # !CONFIG_TRUSTONIC_TEE_SUPPORT

ifneq ($(filter y m,$(CONFIG_MICROTRUST_TEE_SUPPORT)),)
ccflags-y += -I$(srctree)/drivers/tee/teei/$(CONFIG_MICROTRUST_TEE_VERSION)/common/include
endif # !CONFIG_MICROTRUST_TEE_SUPPORT

ifneq ($(filter y m,$(CONFIG_BLOWFISH_TEE_SUPPORT)),)
ccflags-y += -I$(srctree)/drivers/misc/tzdev
endif # !CONFIG_BLOWFISH_TEE_SUPPORT
