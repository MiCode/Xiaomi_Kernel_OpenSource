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

SYS_CFLAGS := \
 -fno-short-enums \
 -funwind-tables \
 -ffunction-sections \
 -fdata-sections \
 -D__linux__

SYS_INCLUDES :=

ifneq ($(TARGET_PLATFORM),)

 # Support for building with the Android NDK >= r15b.
 # The NDK provides only the most basic includes and libraries.

 SYS_INCLUDES += \
  -isystem $(NDK_PLATFORMS_ROOT)/$(TARGET_PLATFORM)/arch-$(TARGET_ARCH)/usr/include \
  -isystem $(NDK_SYSROOT)/usr/include/drm \
  -isystem $(NDK_SYSROOT)/usr/include

else # !TARGET_PLATFORM

 # These libraries are not coming from the NDK now, so we need to include them
 # from the ANDROID_ROOT source tree.

 SYS_INCLUDES += \
  -isystem $(ANDROID_ROOT)/bionic/libc/include \
  -isystem $(ANDROID_ROOT)/bionic/libc/kernel/android/uapi \
  -isystem $(ANDROID_ROOT)/bionic/libc/kernel/uapi \
  -isystem $(ANDROID_ROOT)/bionic/libm/include \
  -isystem $(ANDROID_ROOT)/external/libdrm/include/drm \
  -isystem $(ANDROID_ROOT)/external/zlib \
  -isystem $(ANDROID_ROOT)/frameworks/native/include

 ifeq ($(is_future_version),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/libnativehelper/include_jni \
   -isystem $(ANDROID_ROOT)/system/libfmq/base \
   -isystem $(ANDROID_ROOT)/system/memory/libion/kernel-headers \
   -isystem $(ANDROID_ROOT)/system/memory/libion/kernel-headers/linux \
   -isystem $(ANDROID_ROOT)/system/memory/libion/include \
   -isystem $(ANDROID_ROOT)/system/memory/libdmabufheap/include \
   -isystem $(ANDROID_ROOT)/frameworks/native/libs/gralloc/types/include \
   -isystem $(ANDROID_ROOT)/frameworks/native/libs/binder/ndk/include_ndk \
   -isystem $(ANDROID_ROOT)/frameworks/native/libs/binder/ndk/include_cpp \
   -isystem $(ANDROID_ROOT)/frameworks/native/cmds/dumpstate
 else ifeq ($(is_at_least_r),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/libnativehelper/include_jni \
   -isystem $(ANDROID_ROOT)/system/memory/libion/kernel-headers \
   -isystem $(ANDROID_ROOT)/system/memory/libion/kernel-headers/linux \
   -isystem $(ANDROID_ROOT)/system/memory/libion/include \
   -isystem $(ANDROID_ROOT)/frameworks/native/libs/gralloc/types/include \
   -isystem $(ANDROID_ROOT)/frameworks/native/libs/binder/ndk/include_ndk
 else ifeq ($(is_at_least_q),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/libnativehelper/include_jni \
   -isystem $(ANDROID_ROOT)/system/core/libion \
   -isystem $(ANDROID_ROOT)/system/core/libion/kernel-headers
 else ifeq ($(is_aosp_master),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/libnativehelper/include_jni
 else ifeq ($(is_at_least_oreo),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/libnativehelper/include_jni
 else
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/libnativehelper/include/nativehelper \
   -isystem $(ANDROID_ROOT)/libnativehelper/include
 endif

endif # !TARGET_PLATFORM

ifeq ($(filter-out $(NDK_ROOT)/%,$(NDK_SYSROOT)),)

 # These components aren't in the NDK. They *are* in the VNDK. If this is an
 # NDK or non-NDK build, but not a VNDK build, include the needed bits from
 # the ANDROID_ROOT source tree. We put libsync first because the NDK copy
 # of the sync headers have been stripped in an unsupported way.

 SYS_INCLUDES := \
  -isystem $(ANDROID_ROOT)/system/core/libsync/include \
  $(SYS_INCLUDES) \
  -isystem $(ANDROID_ROOT)/external/libdrm \
  -isystem $(ANDROID_ROOT)/external/libpng \
  -isystem $(ANDROID_ROOT)/hardware/libhardware/include \
  -isystem $(ANDROID_ROOT)/system/core/libion/include \
  -isystem $(ANDROID_ROOT)/system/libhidl/base/include \
  -isystem $(ANDROID_ROOT)/system/libhidl/libhidlmemory/include \
  -isystem $(ANDROID_ROOT)/system/libhidl/transport/include \
  -isystem $(ANDROID_ROOT)/system/libhwbinder/include \
  -isystem $(ANDROID_ROOT)/system/media/camera/include

 # Include headers from libbase
 ifeq ($(is_future_version),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/system/libbase/include
 else
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/system/core/base/include
 endif

 # libadf has been renamed to deprecated-adf from Q
 ifeq ($(is_at_least_q),1)
   SYS_INCLUDES += \
    -isystem $(ANDROID_ROOT)/system/core/deprecated-adf/libadf/include \
    -isystem $(ANDROID_ROOT)/system/core/deprecated-adf/libadfhwc/include
 else
   SYS_INCLUDES += \
    -isystem $(ANDROID_ROOT)/system/core/adf/libadf/include \
    -isystem $(ANDROID_ROOT)/system/core/adf/libadfhwc/include
 endif

 # libjpeg-turbo replaced libjpeg from Nougat
 ifeq ($(is_at_least_nougat),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/external/libjpeg-turbo
 else
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/external/jpeg
 endif

 # Vulkan was only available from Nougat
 ifeq ($(is_at_least_nougat),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/frameworks/native/vulkan/include
 endif

 # Handle upstream includes refactoring
 ifeq ($(is_at_least_oreo),1)
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/frameworks/native/libs/nativewindow/include \
   -isystem $(ANDROID_ROOT)/system/core/libsystem/include \
   -isystem $(ANDROID_ROOT)/system/core/libutils/include
  # Include headers from libbacktrace
  ifeq ($(is_future_version),1)
   SYS_INCLUDES += \
    -isystem $(ANDROID_ROOT)/system/unwinding/libbacktrace/include
  else
   SYS_INCLUDES += \
    -isystem $(ANDROID_ROOT)/system/core/libbacktrace/include
  endif
  ifeq ($(is_at_least_oreo_mr1),1)
   SYS_INCLUDES += \
    -isystem $(ANDROID_ROOT)/frameworks/native/libs/nativebase/include
  endif
  ifeq ($(NDK_ROOT),)
   SYS_INCLUDES += \
    -isystem $(ANDROID_ROOT)/frameworks/native/libs/arect/include
   # Include headers from libcutils/liblog
   ifeq ($(is_future_version),1)
    SYS_INCLUDES += \
     -isystem $(ANDROID_ROOT)/system/core/libcutils/include_outside_system \
     -isystem $(ANDROID_ROOT)/system/logging/liblog/include_vndk
   else
    SYS_INCLUDES += \
     -isystem $(ANDROID_ROOT)/system/core/libcutils/include_vndk \
     -isystem $(ANDROID_ROOT)/system/core/liblog/include_vndk
   endif
   ifeq ($(is_at_least_oreo_mr1),1)
    SYS_INCLUDES += \
     -isystem $(ANDROID_ROOT)/frameworks/ml/nn/driver/cache/BlobCache \
     -isystem $(ANDROID_ROOT)/frameworks/ml/nn/driver/cache/nnCache
   endif
  endif
 else
  SYS_INCLUDES += \
   -isystem $(ANDROID_ROOT)/frameworks/base/include \
   -isystem $(ANDROID_ROOT)/system/core/include
 endif

else # VNDK

 # We're using a VNDK sysroot, but targeting a legacy platform version.
 # In this case, RenderScript can only be built if we pull in headers
 # from the platform build. The user will need that platform to be
 # hanging around.

 ifeq ($(is_at_least_oreo),0)
  SYS_INCLUDES := \
   -isystem $(ANDROID_ROOT)/frameworks/native/include \
   -isystem $(ANDROID_ROOT)/system/core/include \
   $(SYS_INCLUDES)
 endif

endif # !VNDK

# Always include the NDK compatibility directory, because it allows us to
# compile in inline versions of simple functions to eliminate dependencies,
# and we can also constrain the available APIs. Do this last, so we can
# make sure it is always first on the include list.

SYS_INCLUDES := -isystem android/ndk $(SYS_INCLUDES)

OPTIM ?= -O2

# Android enables build-id sections to allow mapping binaries to debug
# information for symbol resolution
SYS_LDFLAGS += -Wl,--build-id=md5 -Wl,--gc-sections
