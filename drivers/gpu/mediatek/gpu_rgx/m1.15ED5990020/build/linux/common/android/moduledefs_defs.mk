########################################################################### ###
#@File
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

# System libraries will be installed at runtime if APEX is enabled and
# libraries in system/lib{64} are soft links to /system/apex which is not
# available at build time.
#
define path-to-system-library
$(call if-exists,$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/$(1)/bootstrap/lib$(2).so,\
                 $(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/$(1)/lib$(2).so)
endef

_vndk := vndk
_vndk-sp := vndk-sp

# VNDK and VNDK-SP library location depends on android version and
# treble support.
# From android Pie (API_LEVEL = 28) and above, vndk and vndk-sp
# directory names have API_LEVEL postfix, eg. vndk-28.
#
define path-to-vndk
$(if $(filter true,$(SUPPORT_TREBLE)),\
  $(if $(shell (test $(API_LEVEL) -ge 28) && echo 1 || echo 0),\
  $(if $(wildcard $(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/$(1)/$(_vndk)-$(PLATFORM_CODENAME)),\
  $(_vndk)-$(PLATFORM_CODENAME),$(_vndk)-$(API_LEVEL)),$(_vndk)),)
endef

define path-to-vndk-sp
$(if $(filter true,$(SUPPORT_TREBLE)),\
  $(if $(shell (test $(API_LEVEL) -ge 28) && echo 1 || echo 0),\
  $(if $(wildcard $(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/$(1)/$(_vndk-sp)-$(PLATFORM_CODENAME)),\
  $(_vndk-sp)-$(PLATFORM_CODENAME),$(_vndk-sp)-$(API_LEVEL)),$(_vndk-sp)),$(_vndk-sp))
endef

# From Android 11 (API_LEVEL = 30), the folder of vndk libraries moves to
# a different location depending on the combination of AOSP build configs:
# 1) TARGET_FLATTEN_APEX is true:
#   - The libraries are installed at system/apex/com.android.vndk.current/
# 2) TARGET_FLATTEN_APEX is false and PRODUCT_INSTALL_EXTRA_FLATTENED_APEXES is true:
#   - The libraries are installed at system/system_ext/apex/com.android.vndk.current/
# 3) Both TARGET_FLATTEN_APEX and PRODUCT_INSTALL_EXTRA_FLATTENED_APEXES are false:
#   - The libraries are installed at apex/com.android.vndk.v$(API_LEVEL) or
#     apex/com.android.vndk.v$(PLATFORM_CODENAME)
#
define path-to-apex-vndk
$(if $(filter true,$(SUPPORT_TREBLE)),\
  $(if $(shell (test $(API_LEVEL) -ge 30) && echo 1 || echo 0),\
   $(if $(wildcard $(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/apex/com.android.vndk.current/$(1)),\
    system/apex/com.android.vndk.current,\
    $(if $(wildcard $(TARGET_ROOT)/product/$(TARGET_DEVICE)/apex/com.android.vndk.v$(API_LEVEL)/$(1)),\
     apex/com.android.vndk.v$(API_LEVEL),\
     $(if $(wildcard $(TARGET_ROOT)/product/$(TARGET_DEVICE)/apex/com.android.vndk.v$(PLATFORM_CODENAME)/$(1)),\
      apex/com.android.vndk.v$(PLATFORM_CODENAME),system/system_ext/apex/com.android.vndk.current))),),)
endef

# Android Pie
_last_aosp_api_level := 28
is_at_least_q := $(shell (test $(API_LEVEL) -gt $(_last_aosp_api_level)) && echo 1 || echo 0)

_ndk_platforms := $(ANDROID_ROOT)/prebuilts/ndk/current/platforms
_sdk_runtime := $(ANDROID_ROOT)/prebuilts/runtime/mainline/runtime/sdk/android

# LIBC runtime should be in $(TARGET_ROOT)/product/$(TARGET_DEVICE)/obj.
# Building DDK against AOSP master or above, LIBC runtime may not be there
# anymore so linking to crt from prebuilts/ndk instead. If NDK version isn't
# shipping the corresponding platforms, then linking to the latest
# AOSP ones.
#
define path-to-libc-rt
$(if $(wildcard $(1)/lib),$(1),$(call if-exists,\
                                $(_ndk_platforms)/android-$(API_LEVEL)/arch-$(2)/usr,\
                                $(call if-exists,\
                                 $(_ndk_platforms)/android-$(_last_aosp_api_level)/arch-$(2)/usr,
                                 $(_sdk_runtime)/$(2))))
endef
