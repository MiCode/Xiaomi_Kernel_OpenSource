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

ifeq ($(__included_paths_mk),)

__included_paths_mk := true

# If this is an SDK build, set it up to use the NDK bundle from the SDK, which
# turns it into an NDK build too. The user can set NDK_ROOT to something else
# if they want to use a different NDK. This sets up a default for ANDROID_ROOT
# too, which can also be overridden.
#
ifneq ($(ANDROID_SDK_ROOT),)
NDK_ROOT ?= $(ANDROID_SDK_ROOT)/ndk-bundle
ANDROID_ROOT ?= $(ANDROID_SDK_ROOT)/vndk
endif

OUT_DIR ?= $(ANDROID_ROOT)/out

LLVM_LIB_VERSION = 90

# If this is an NDK build, set it up to use a VNDK platform/sysroot from the
# OUT_DIR, if a suitable directory exists in the default location. The user
# can set OUT_DIR or VNDK_ROOT to something else if they want to override
# this.
#
ifneq ($(NDK_ROOT),)
ifneq ($(wildcard $(OUT_DIR)/soong/ndk),)
VNDK_ROOT ?= $(OUT_DIR)/soong/ndk
endif
endif

TARGET_BUILD_TYPE ?= release

HOST_OS ?= linux
HOST_ARCH ?= x86_64
HOST_PREBUILT_ARCH ?= x86

ifeq ($(TARGET_BUILD_TYPE),debug)
TARGET_ROOT := $(OUT_DIR)/debug/target
else
TARGET_ROOT := $(OUT_DIR)/target
endif

ifeq ($(NDK_ROOT),)

# Non-NDK build method

LIBCXX_INCLUDE_PATH := \
	$(ANDROID_ROOT)/external/libcxx/include

LLVM_LIB_PATH  := $(ANDROID_DDK_DEPS)/out/local/_LLVM_ARCH_/llvm$(LLVM_LIB_VERSION)
CLANG_LIB_PATH := $(ANDROID_DDK_DEPS)/out/local/_LLVM_ARCH_/llvm$(LLVM_LIB_VERSION)

LLVM_INCLUDE_PATH  := \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/target_build/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include

CLANG_INCLUDE_PATH := \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/target_build/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/target_build/tools/clang/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/clang/include

SPV_INCLUDE_PATH := \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/spv-translator/include

LLVM_INCLUDE_PATH_HOST  := \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/host_build/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include

CLANG_INCLUDE_PATH_HOST := \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/host_build/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/host_build/tools/clang/include \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/clang/include

SPV_INCLUDE_PATH_HOST := \
	$(ANDROID_DDK_DEPS)/apps/llvm$(LLVM_LIB_VERSION)/spv-translator/include

ifeq ($(wildcard $(ANDROID_DDK_DEPS)/apps/nnvm00),)
  # IMGDNN is not built by default so do nothing if NNVM doesn't exist
else
	NNVM_INCLUDE_PATH := $(ANDROID_DDK_DEPS)/apps/nnvm00/include
	NNVM_LIB_PATH := $(ANDROID_DDK_DEPS)/out/local/_NNVM_ARCH_/nnvm00
endif

LLVM_LIB_PATH_HOST := $(ANDROID_DDK_DEPS)/out/local/host/lib/llvm$(LLVM_LIB_VERSION)

else # !NDK_ROOT

# NDK build method

# Built using new system c++ STL
LIBCXX_INCLUDE_PATH := \
	$(NDK_ROOT)/apps/libcxx/include

# Built using ndk-build / 'local' method
LLVM_LIB_PATH  := $(NDK_ROOT)/out/local/_LLVM_ARCH_/llvm$(LLVM_LIB_VERSION)
CLANG_LIB_PATH := $(NDK_ROOT)/out/local/_LLVM_ARCH_/llvm$(LLVM_LIB_VERSION)

LLVM_INCLUDE_PATH  := \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/target_build/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include

CLANG_INCLUDE_PATH := \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/target_build/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/target_build/tools/clang/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/clang/include

SPV_INCLUDE_PATH := \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/spv-translator/include

LLVM_INCLUDE_PATH_HOST  := \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/host_build/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include

CLANG_INCLUDE_PATH_HOST := \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/host_build/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/llvm/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/host_build/tools/clang/include \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/clang/include

SPV_INCLUDE_PATH_HOST := \
	$(NDK_ROOT)/apps/llvm$(LLVM_LIB_VERSION)/spv-translator/include

ifeq ($(wildcard $(NDK_ROOT)/apps/nnvm00),)
  # IMGDNN is not built by default so do nothing if NNVM doesn't exist
else
	NNVM_INCLUDE_PATH := $(NDK_ROOT)/apps/nnvm00/include
	NNVM_LIB_PATH := $(NDK_ROOT)/out/local/_NNVM_ARCH_/nnvm00
endif

LLVM_LIB_PATH_HOST := "${NDK_ROOT}/out/local/host/lib/llvm$(LLVM_LIB_VERSION)"

endif # !NDK_ROOT

endif # !__included_paths_mk
