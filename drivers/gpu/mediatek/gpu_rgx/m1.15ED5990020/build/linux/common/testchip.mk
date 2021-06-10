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

XE_BVNC = $(shell echo $(RGX_BVNC) | grep '^22.*\|^24.*\|^29.*\|^36.*')

ifeq ($(RGX_BVNC),$(XE_BVNC))
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5,))
 SUPPORT_FPGA_DUT_CLK_INFO ?= 1
endif

ifeq ($(PVR_ARCH),volcanic)
 SUPPORT_FPGA_DUT_CLK_INFO ?= 1
endif

$(eval $(call TunableKernelConfigC,SUPPORT_FPGA_DUT_CLK_INFO,))

ifeq ($(RGX_BVNC),1.82.4.5)
 $(eval $(call KernelConfigC,TC_APOLLO_ES2,))
else ifeq ($(RGX_BVNC),4.31.4.55)
 $(eval $(call KernelConfigC,TC_APOLLO_BONNIE,))
else ifeq ($(RGX_BVNC),22.46.54.330)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_46_54_330,))
else ifeq ($(RGX_BVNC),22.49.21.16)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_49_21_16,))
else ifeq ($(RGX_BVNC),22.60.22.29)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_60_22_29,))
else ifeq ($(RGX_BVNC),22.67.54.30)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_67_54_30,))
else ifeq ($(RGX_BVNC),22.75.22.25)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_75_22_25,))
else ifeq ($(RGX_BVNC),22.86.104.218)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_86_104_218,))
else ifeq ($(RGX_BVNC),22.88.104.318)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_88_104_318,))
else ifeq ($(RGX_BVNC),22.89.204.18)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_89_204_18,))
else ifeq ($(RGX_BVNC),22.98.54.230)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_98_54_230,))
else ifeq ($(RGX_BVNC),22.102.54.38)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_22_102_54_38,))
else ifeq ($(RGX_BVNC),33.8.22.1)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_33_8_22_1,))
else ifeq ($(RGX_BVNC),29.18.204.508)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_29_18_204_508,))
else ifeq ($(RGX_BVNC),29.12.52.208)
 $(eval $(call KernelConfigC,TC_ORION,))
else ifeq ($(RGX_BVNC),29.19.52.202)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_29_19_52_202,))
else ifeq ($(RGX_BVNC),$(XE_BVNC))
 $(warning WARNING $(RGX_BVNC) is currently not supported on a Linux TCF5 system)
 $(eval $(call KernelConfigC,TC_APOLLO_TCF5_BVNC_NOT_SUPPORTED,))
endif

ifneq ($(SUPPORT_ANDROID_PLATFORM),)
 ifneq ($(PVR_SYSTEM),rgx_nohw)
  override TC_MEMORY_CONFIG := TC_MEMORY_LOCAL
 endif
endif

ifneq ($(PVR_SYSTEM),rgx_nohw)
 ifeq ($(TC_MEMORY_CONFIG),)
  $(error TC_MEMORY_CONFIG must be defined)
 endif
endif

$(eval $(call TunableBothConfigC,TC_MEMORY_CONFIG,$(TC_MEMORY_CONFIG),\
Selects the memory configuration to be used. The choices are:_\
* TC_MEMORY_LOCAL (Rogue and the display controller use local card memory)_\
* TC_MEMORY_HOST (Rogue and the display controller use system memory)_\
* TC_MEMORY_HYBRID (Rogue can use both system and local memory and the display controller uses local card memory)))

ifeq ($(TC_FAKE_INTERRUPTS),1)
$(eval $(call KernelConfigC,TC_FAKE_INTERRUPTS,))
endif

ifeq ($(TC_MEMORY_CONFIG),TC_MEMORY_LOCAL)
 ifeq ($(TC_DISPLAY_MEM_SIZE),)
  $(error TC_DISPLAY_MEM_SIZE must be set in $(PVR_BUILD_DIR)/Makefile)
 endif
 $(eval $(call KernelConfigC,TC_DISPLAY_MEM_SIZE,$(TC_DISPLAY_MEM_SIZE)))
 $(eval $(call BothConfigC,LMA,))
 $(eval $(call KernelConfigMake,LMA,1))
else ifeq ($(TC_MEMORY_CONFIG),TC_MEMORY_HYBRID)
 ifeq ($(TC_DISPLAY_MEM_SIZE),)
  $(error TC_DISPLAY_MEM_SIZE must be set in $(PVR_BUILD_DIR)/Makefile)
 endif
 $(eval $(call KernelConfigC,TC_DISPLAY_MEM_SIZE,$(TC_DISPLAY_MEM_SIZE)))
endif

$(eval $(call TunableKernelConfigC,SUPPORT_APOLLO_FPGA,))

$(eval $(call TunableKernelConfigC,SUPPORT_FAKE_SECURE_ION_HEAP,))
$(eval $(call TunableKernelConfigC,TC_SECURE_MEM_SIZE,128))
