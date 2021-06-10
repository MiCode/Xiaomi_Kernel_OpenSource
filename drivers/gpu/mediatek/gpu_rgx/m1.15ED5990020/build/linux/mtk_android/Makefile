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

include ../config/preconfig.mk

export HAL_VARIANT ?= rogue
export SUPPORT_ION := 1
export PVR_SYSTEM := rgx_mtk
export ION_DEFAULT_HEAP_NAME := \"ion_system_heap\"
export ION_DEFAULT_HEAP_ID_MASK := (1 << ION_HEAP_TYPE_SYSTEM)
export PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC := 1

# MTK export variables
# decouple: MTK mini porting part
export MTK_MINI_PORTING ?= 1

# decouple: MTK full porting part
ifeq ($(MTK_MINI_PORTING),)
 export MTK_FULL_PORTING := 0
else
 export MTK_FULL_PORTING ?= 1
endif

# feature: HW APM | Bring-up mark-off
ifeq ($(MTK_FULL_PORTING),1)
ifeq ($(MTK_BRINGUP),1)
 export MTK_USE_HW_APM ?= 0
else
 export MTK_USE_HW_APM ?= 1
endif
else
 export MTK_USE_HW_APM := 0
endif

# feature: mtk debug log option, only enable in eng load
ifeq ($(MTK_FULL_PORTING),1)
 export MTK_DEBUG ?= 0
else
 export MTK_DEBUG := 0
endif

# feature: a ring buffer for debug
ifeq ($(MTK_FULL_PORTING),1)
 export MTK_DEBUG_PROC_PRINT ?= 1
else
 export MTK_DEBUG_PROC_PRINT := 0
endif

# feature: WA for avoid to poll timeout at power on/off.
ifeq ($(MTK_FULL_PORTING),1)
 export MTK_POWER_OFF_TIMING ?= 0
else
 export MTK_POWER_OFF_TIMING := 0
endif

# feature: mtk power ralated debug
ifeq ($(MTK_FULL_PORTING),1)
 export MTK_CAL_POWER_INDEX ?= 0
else
 export MTK_CAL_POWER_INDEX := 0
endif

# feature: device tree support
ifeq ($(MTK_MINI_PORTING),1)
 export MTK_CONFIG_OF ?= 1
else
 export MTK_CONFIG_OF := 0
endif

# feature: mtk common dvfs support
ifeq ($(MTK_MINI_PORTING),1)
 export ENABLE_COMMON_DVFS ?= 1
else
 export ENABLE_COMMON_DVFS := 0
endif

# feature: gpu bandwidth prediction (QOS 2.0)
ifeq ($(MTK_FULL_PORTING),1)
 export MTK_GPU_BM_SUPPORT ?= 0
else
 export MTK_GPU_BM_SUPPORT := 0
endif

include ../common/android/paths.mk
include ../common/android/arch.mk
include ../common/android/features.mk

ifneq ($(strip $(KERNELDIR)),)
 include ../config/kernel_version.mk
 ifeq ($(call kernel-version-at-least,2,6,39),true)
  ifeq ($(LDM_PLATFORM),1)
   export PVR_LDM_PLATFORM_PRE_REGISTERED := 0
  endif
 endif
endif

# export DISPLAY_CONTROLLER ?= dc_fbdev
# export KERNEL_COMPONENTS += $(DISPLAY_CONTROLLER)

export CACHEFLUSH_NO_KMRBF_USING_UMVA ?= 1

export DC_FBDEV_NUM_PREFERRED_BUFFERS := 3

include ../config/core.mk
include ../common/3rdparty.mk
include ../common/android/extra_config.mk

$(eval $(call TunableBothConfigC,MTK_MINI_PORTING,))
$(eval $(call TunableBothConfigC,MTK_FULL_PORTING,))
$(eval $(call TunableBothConfigC,MTK_BRINGUP,))
$(eval $(call TunableBothConfigC,MTK_USE_HW_APM,))
$(eval $(call TunableBothConfigC,MTK_DEBUG,))
$(eval $(call TunableBothConfigC,MTK_DEBUG_PROC_PRINT,))
$(eval $(call TunableBothConfigC,MTK_POWER_OFF_TIMING,))
$(eval $(call TunableBothConfigC,MTK_CAL_POWER_INDEX,))
$(eval $(call TunableBothConfigC,MTK_CONFIG_OF))
$(eval $(call TunableBothConfigC,ENABLE_COMMON_DVFS))
$(eval $(call TunableBothConfigC,MTK_GPU_BM_SUPPORT,))
