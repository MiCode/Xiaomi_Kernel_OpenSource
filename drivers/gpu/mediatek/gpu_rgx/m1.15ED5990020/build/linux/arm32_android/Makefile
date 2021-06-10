########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
#
# The contents of this file are subject to the MIT license as set out below.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
#
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
#
# This License is also included in this distribution in the file called
# "MIT-COPYING".
#
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

# This uses rk3368_arm32 as an arm32 build replacement. This will be replaced when we
# have an actual arm32 based v2 system.
#

ifeq ($(TARGET_DEVICE),)
override TARGET_DEVICE := \
 $(patsubst mini_%,%,$(patsubst full_%,%,$(TARGET_PRODUCT)))
ifneq ($(filter kylin_perf kylin_p1,$(TARGET_DEVICE)),)
override TARGET_DEVICE := rk3368
endif
endif

ifeq ($(TARGET_DEVICE),)
override TARGET_DEVICE := rk3368
endif

# Buckle BVNC:
RGX_BVNC ?= 20.3.204.3004

include ../config/preconfig.mk
include ../common/android/arch.mk
include ../common/android/features.mk


ifeq ($(TARGET_DEVICE),rk3368)
PVR_SYSTEM := rk3368

# Kernel for Geekbox does not support llvm tools.
ifeq ($(KERNEL_CC),clang)
override KERNEL_LD :=
override KERNEL_NM :=
override KERNEL_OBJCOPY :=
endif
else
PVR_SYSTEM := rgx_sunxi
KERNEL_CROSS_COMPILE ?= arm-eabi-
endif

# Heap names should be defined as per the labels used in the kernel.
# Kernels >=4.12, support heap query so we use heap names
# to get the correct heap id.
# For kernels <4.12, we use heap_id_mask defined in this Makefile.
# Heap names defined here are for reference only.
ION_DEFAULT_HEAP_NAME := \"cma\"
ION_DEFAULT_HEAP_ID_MASK := (1 << ION_HEAP_TYPE_DMA)
ION_FALLBACK_HEAP_NAME := \"carveout\"
ION_FALLBACK_HEAP_ID_MASK := (1 << ION_HEAP_TYPE_CARVEOUT)

ifeq ($(TARGET_DEVICE),rk3368)
PVR_ANDROID_COMPOSERHAL ?= drm
ifeq ($(PVR_ANDROID_COMPOSERHAL),drm)
PVR_DRM_MODESET_MODULE_NAME := dumb
PVR_DRM_MODESET_DRIVER_NAME := rockchip
override PVR_LDM_PLATFORM_PRE_REGISTERED := 1
endif
else
# ADF libs are no longer present in rk3368 package due to treble limitations.
# Set composerhal to null to prevent ADF build issues in
# ARM32 kernel + ARM64 user build passes.
ifneq ($(TARGET_DEVICE),sun9i)
 override PVR_ANDROID_COMPOSERHAL := null
endif

DISPLAY_CONTROLLER ?= adf_fbdev
KERNEL_COMPONENTS += $(DISPLAY_CONTROLLER)
endif

DEVICE_MEMSETCPY_ALIGN_IN_BYTES := 8

RGX_TIMECORR_CLOCK := sched

# Don't use dma_alloc_coherent because it falls back to allocate
# highmem pages and maps them to vmalloc space which exhausts quickly
PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC := 1

override PVRSRV_ENABLE_PVR_ION_STATS := 0

include ../config/core.mk
include ../common/android/extra_config.mk

ifeq ($(TARGET_DEVICE),rk3368)
ifeq ($(call kernel-version-at-least,4,4),true)
$(eval $(call KernelConfigMake,RK_INIT_V2,1))
endif
endif
