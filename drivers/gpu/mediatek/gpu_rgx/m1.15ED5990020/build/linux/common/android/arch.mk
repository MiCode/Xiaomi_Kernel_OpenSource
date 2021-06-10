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

include ../common/android/platform_version.mk

TARGET_ARCH ?= $(ARCH)

# Remap kbuild architectures that do not match Android architectures

ifeq ($(ARCH),i386)
 TARGET_ARCH := x86
endif

# Remap primary architectures with identically-named secondary architectures
# If you want to build for MIPS64 *only* you will *have* to set both ARCH
# and TARGET_ARCH manually.

ifeq ($(TARGET_2ND_ARCH),mips)
 TARGET_ARCH := mips64
endif

# If the TARGET_ARCH wasn't possible to detect (user did not set kbuild ARCH)
# then we might be able to use the BUILD_PROP to infer it. However, this is
# only possible when there *is* a build.prop.

ifeq ($(TARGET_ARCH),)
 ifneq ($(BUILD_PROP),)
  $(eval $(subst #,$(newline),$(shell cat $(BUILD_PROP) | \
      grep '^ro.product.cpu.abilist=\|^ro.product.cpu.abilist32=' | \
      sed -e 's,ro.product.cpu.abilist=,JNI_CPU_ABI=,' \
          -e 's,ro.product.cpu.abilist32=,JNI_CPU_ABI_2ND=,' | \
      tr ',' ' ' | tr '\n' '#')))
  ifneq ($(filter arm64-v8a,$(JNI_CPU_ABI)),)
   TARGET_ARCH := arm64
  else ifneq ($(filter armeabi-v7a armeabi,$(JNI_CPU_ABI)),)
   TARGET_ARCH := arm
  else ifneq ($(filter mips64,$(JNI_CPU_ABI)),)
   TARGET_ARCH := mips64
  else ifneq ($(filter mips,$(JNI_CPU_ABI)),)
   TARGET_ARCH := mips
  else ifneq ($(filter x86_64,$(JNI_CPU_ABI)),)
   TARGET_ARCH := x86_64
  else ifneq ($(filter x86,$(JNI_CPU_ABI)),)
   TARGET_ARCH := x86
  else
   $(error TARGET_ARCH was not set and JNI_CPU_ABI=$(JNI_CPU_ABI) was not remappable)
  endif
  JNI_CPU_ABI := $(word 1,$(JNI_CPU_ABI))
  JNI_CPU_ABI_2ND := $(word 1,$(JNI_CPU_ABI_2ND))
 endif
endif

ifeq ($(TARGET_ARCH),)
 $(error TARGET_ARCH was not set and build.prop was not available to infer it)
endif

# Set up some defaults for the secondary architecture. This prefers multiarch
# builds, but you can still set MULTIARCH=64only to disable it. We can also
# use this block to validate TARGET_ARCH, and set-up the JNI ABIs if they are
# unset.

ifeq ($(TARGET_ARCH),arm64)
 ifneq ($(MULTIARCH),64only)
  TARGET_2ND_ARCH ?= arm
  JNI_CPU_ABI_2ND ?= armeabi-v7a
 endif
 JNI_CPU_ABI      ?= arm64-v8a
else ifeq ($(TARGET_ARCH),mips64)
 ifneq ($(MULTIARCH),64only)
  TARGET_2ND_ARCH ?= mips
  JNI_CPU_ABI_2ND ?= mips
 endif
 JNI_CPU_ABI      ?= mips64
else ifeq ($(TARGET_ARCH),x86_64)
 ifneq ($(MULTIARCH),64only)
  TARGET_2ND_ARCH ?= x86
  JNI_CPU_ABI_2ND ?= x86
 endif
 JNI_CPU_ABI      ?= x86_64
else ifeq ($(TARGET_ARCH),arm)
 JNI_CPU_ABI      ?= armeabi-v7a
else ifeq ($(TARGET_ARCH),mips)
 JNI_CPU_ABI      ?= mips
else ifeq ($(TARGET_ARCH),x86)
 JNI_CPU_ABI      ?= x86
else
 $(error Unsupported primary architecture TARGET_ARCH=$(TARGET_ARCH))
endif

# If TARGET_2ND_ARCH is unset by this point, it's either a pure 32-bit
# architecture or MULTIARCH was set to 64only. Don't validate the
# TARGET_2ND_ARCH or fiddle with MULTIARCH in this case.

ifneq ($(TARGET_2ND_ARCH),)
 ifeq ($(filter arm mips x86,$(TARGET_2ND_ARCH)),)
  $(error Unsupported secondary architecture TARGET_2ND_ARCH=$(TARGET_2ND_ARCH))
 endif
 $(warning *** 64-bit architecture detected. Enabling MULTIARCH=1.)
 $(warning *** If you want a 64-bit only build, use MULTIARCH=64only.)
 export MULTIARCH := 1
endif

include ../common/android/arch_common.mk

ifneq ($(filter x86 x86_64,$(TARGET_ARCH)),)
KERNEL_CROSS_COMPILE ?= undef
endif
