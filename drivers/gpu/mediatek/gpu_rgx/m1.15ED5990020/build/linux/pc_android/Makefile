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

# If a TARGET_PRODUCT is specified but not a TARGET_DEVICE, try to
# derive the TARGET_DEVICE from TARGET_PRODUCT.
#
ifeq ($(TARGET_DEVICE),)
override TARGET_DEVICE := \
 $(patsubst mini_%,%,$(patsubst full_%,%,$(TARGET_PRODUCT)))
endif

ifeq ($(TARGET_DEVICE),)
override TARGET_DEVICE := pc
endif

# TC 2:
RGX_BVNC ?= 1.82.4.5
# TC BONNIE:
#RGX_BVNC ?= 4.31.4.55
# Buckle BVNC:
#RGX_BVNC ?= 20.3.204.3004

include ../config/preconfig.mk
include ../common/android/arch.mk
include ../common/android/features.mk

ifeq ($(NO_HARDWARE),1)
 PVR_SYSTEM := rgx_nohw
 ifeq ($(PVR_ARCH),rogue)
  ION_DEFAULT_HEAP_NAME := \"ion_system_heap\"
 endif
else
 PVR_SYSTEM := rgx_linux_tc
 PVR_LDM_PLATFORM_PRE_REGISTERED := 1
 # Heap names should be defined as per the labels used in the kernel.
 # Kernels >=4.12, support heap query so we use heap names
 # to get the correct heap id.
 # For kernels <4.12, we use heap_id_mask defined in this Makefile.
 # Heap names defined here are for reference only.
 ION_DEFAULT_HEAP_NAME := \"tc-pdp\"

 # For kernel >= 5.4, dmabuf heap is supported.
 ifeq ($(PVR_ANDROID_HAS_LIBDMABUFHEAP),1)
 DMABUF_DEFAULT_HEAP_NAME := \"tc-pdp\"
 endif

 ION_DEFAULT_HEAP_ID_MASK := (1 << (ION_HEAP_TYPE_CUSTOM + 2))

 ifeq ($(PVR_ARCH),volcanic)
  TC_MEMORY_CONFIG ?= TC_MEMORY_HYBRID
  PVRSRV_APPHINT_FABRICCOHERENCYOVERRIDE := 0
 endif

 override TC_DISPLAY_MEM_SIZE := 384
endif

ifeq ($(PVR_ARCH),volcanic)
 # Turn this on for out of the box vnc support
 #PVR_ANDROID_VNC ?= 1

 PVR_ANDROID_COMPOSERHAL ?= drm
 PVR_ANDROID_USE_PDP_LEGACY ?= 1

 ifneq ($(NO_HARDWARE),1)
  ifeq ($(PVR_ANDROID_VNC),1)
   PVR_ANDROID_VNC_WIDTH ?= 240
   PVR_ANDROID_VNC_HEIGHT ?= 320
   PVR_ANDROID_VNC_PORT ?= 5900
  else ifeq ($(PVR_ANDROID_COMPOSERHAL),drm)
   DISPLAY_CONTROLLER ?= drm_pdp
   PVR_DRM_MODESET_DRIVER_NAME := pdp
   PVR_ANDROID_COMPOSERHAL_OVERRIDE_DEFAULT_MODE := 1
   PVR_ANDROID_COMPOSERHAL_OVERRIDE_DEFAULT_MODE_HEIGHT := 720
   PVR_ANDROID_COMPOSERHAL_OVERRIDE_DEFAULT_MODE_WIDTH := 1280
  else
   DISPLAY_CONTROLLER ?= adf_pdp
  endif
  KERNEL_COMPONENTS += tc $(DISPLAY_CONTROLLER)
 endif
else
 PVR_ANDROID_COMPOSERHAL ?= drm
 ifneq ($(call kernel-version-at-least,4,14),true)
  PVR_ANDROID_USE_PDP_LEGACY ?= 1
 endif

 ifneq ($(NO_HARDWARE),1)
  ifeq ($(VIRTUAL_PLATFORM),1)
   DISPLAY_CONTROLLER ?= adf_pdp
  else
   ifeq ($(PVR_ANDROID_COMPOSERHAL),drm)
    DISPLAY_CONTROLLER ?= drm_pdp
    PVR_DRM_MODESET_DRIVER_NAME := pdp
    PVR_ANDROID_COMPOSERHAL_OVERRIDE_DEFAULT_MODE := 1
    PVR_ANDROID_COMPOSERHAL_OVERRIDE_DEFAULT_MODE_HEIGHT := 720
    PVR_ANDROID_COMPOSERHAL_OVERRIDE_DEFAULT_MODE_WIDTH := 1280
   else
    DISPLAY_CONTROLLER ?= adf_pdp
   endif
  endif
  KERNEL_COMPONENTS += tc $(DISPLAY_CONTROLLER)
 endif
endif

ifeq ($(SUPPORT_RGX),0)
KERNEL_COMPONENTS := $(filter-out srvkm,$(KERNEL_COMPONENTS))
endif

ifeq ($(ARCH),i386)
ifeq ($(KERNEL_CC),clang)
DEVICE_MEMSETCPY_ALIGN_IN_BYTES := 8
endif
endif

# Turn this on to clear FBCDC memory in kernel mode
# PVR_ANDROID_DEFER_CLEAR needs to be turned off
#PVR_ANDROID_ION_FBCDC_ALLOC ?= 1

PVR_ANDROID_DEFER_CLEAR ?= 1
TC_FAKE_INTERRUPTS ?= 0

ifeq ($(SUPPORT_FAKE_SECURE_ION_HEAP),1)
  ION_SECURE_HEAP_ID_MASK := (1 << (ION_HEAP_TYPE_CUSTOM + 3))
  ION_SECURE_HEAP_NAME := \"tc-secure\"
  PVR_ANDROID_GRALLOC_ALLOC_SECURE := 1
endif

include ../config/core.mk
include ../common/android/extra_config.mk
include ../common/3rdparty.mk
include ../common/testchip.mk

# An option to enable/disable atomic mode-setting in kernel
$(eval $(call TunableKernelConfigC,PVR_ANDROID_USE_PDP_LEGACY,\
              $(PVR_ANDROID_USE_PDP_LEGACY)))

ifeq ($(PVR_ARCH),rogue)
 # For VP this needs to be external visible options
 $(eval $(call KernelConfigC,TC_APOLLO_ES2,))
 $(eval $(call TunableKernelConfigC,VIRTUAL_PLATFORM,))
endif
