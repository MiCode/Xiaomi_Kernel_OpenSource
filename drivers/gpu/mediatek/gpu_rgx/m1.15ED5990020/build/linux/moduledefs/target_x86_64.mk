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

MODULE_AR := $(AR)
MODULE_CC := $(CC) -march=x86-64
MODULE_CXX := $(CXX) -march=x86-64
MODULE_NM := $(NM)
MODULE_OBJCOPY := $(OBJCOPY)
MODULE_RANLIB := $(RANLIB)
MODULE_STRIP := $(STRIP)

MODULE_CFLAGS := $(ALL_CFLAGS) $($(THIS_MODULE)_cflags) -march=x86-64 -mstackrealign
MODULE_CXXFLAGS := $(ALL_CXXFLAGS) $($(THIS_MODULE)_cxxflags) -march=x86-64 -mstackrealign
MODULE_LDFLAGS := $($(THIS_MODULE)_ldflags) -L$(MODULE_OUT) -Xlinker -rpath-link=$(MODULE_OUT) $(ALL_LDFLAGS)

# Since this is a target module, add system-specific include flags.
MODULE_INCLUDE_FLAGS := \
 $(SYS_INCLUDES_RESIDUAL) \
 $(addprefix -isystem ,$(filter-out $(patsubst -I%,%,$(filter -I%,$(MODULE_INCLUDE_FLAGS))),$(SYS_INCLUDES_ISYSTEM))) \
 $(MODULE_INCLUDE_FLAGS)

ifneq ($(SUPPORT_ANDROID_PLATFORM),)

MODULE_EXE_LDFLAGS := \
 -Bdynamic -nostdlib -Wl,-dynamic-linker,/system/bin/linker64

MODULE_LIBGCC := -Wl,--version-script,$(MAKE_TOP)/common/libgcc.lds $(LIBGCC)

ifeq ($(NDK_ROOT),)

include $(MAKE_TOP)/common/android/moduledefs_defs.mk

_obj := $(TARGET_ROOT)/product/$(TARGET_DEVICE)/obj
_lib := lib64

SYSTEM_LIBRARY_LIBC  := $(strip $(call path-to-system-library,$(_lib),c))
SYSTEM_LIBRARY_LIBM  := $(strip $(call path-to-system-library,$(_lib),m))
SYSTEM_LIBRARY_LIBDL := $(strip $(call path-to-system-library,$(_lib),dl))

ifeq ($(USE_LLD),1)
 MODULE_LDFLAGS += -fuse-ld=lld
endif

MODULE_EXE_LDFLAGS += $(SYSTEM_LIBRARY_LIBC)

# APK unittests
ifneq (,$(findstring $(THIS_MODULE),$(PVR_UNITTESTS_APK)))
MODULE_SYSTEM_LIBRARY_DIR_FLAGS := \
 -L$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64 \
 -Xlinker -rpath-link=$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64

MODULE_LIBRARY_FLAGS_SUBST += \
 c++_static:$(ANDROID_DDK_DEPS)/out/local/x86_64/libc++_static.a$$(space)$(ANDROID_DDK_DEPS)/out/local/x86_64/libc++abi.a \
 RScpp:$(_obj)/STATIC_LIBRARIES/libRScpp_static_intermediates/libRScpp_static.a

MODULE_EXE_LDFLAGS := $(MODULE_EXE_LDFLAGS) $(LIBGCC) -Wl,--as-needed $(SYSTEM_LIBRARY_LIBDL)

else

_vndk := $(strip $(call path-to-vndk,$(_lib)))
_vndk-sp := $(strip $(call path-to-vndk-sp,$(_lib)))
_apex-vndk := $(strip $(call path-to-apex-vndk,$(_lib)))

MODULE_SYSTEM_LIBRARY_DIR_FLAGS := \
 -L$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/$(_vndk) \
 -Xlinker -rpath-link=$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/$(_vndk) \
 -L$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/$(_vndk-sp) \
 -Xlinker -rpath-link=$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/$(_vndk-sp) \
 -L$(TARGET_ROOT)/product/$(TARGET_DEVICE)/$(_apex-vndk)/lib64 \
 -Xlinker -rpath-link=$(TARGET_ROOT)/product/$(TARGET_DEVICE)/$(_apex-vndk)/lib64

# Vendor libraries are required for gralloc, hwcomposer, and proprietary HIDL HALs.
MODULE_VENDOR_LIBRARY_DIR_FLAGS := \
 -L$(TARGET_ROOT)/product/$(TARGET_DEVICE)/vendor/lib64 \
 -Xlinker -rpath-link=$(TARGET_ROOT)/product/$(TARGET_DEVICE)/vendor/lib64

# LL-NDK libraries
ifneq ($(PVR_ANDROID_LLNDK_LIBRARIES),)
MODULE_LIBRARY_FLAGS_SUBST := \
 $(foreach _llndk,$(PVR_ANDROID_LLNDK_LIBRARIES), \
  $(_llndk):$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/lib$(_llndk).so)
endif

# CLDNN needs libneuralnetworks_common.a
MODULE_LIBRARY_FLAGS_SUBST += \
 neuralnetworks_common:$(_obj)/STATIC_LIBRARIES/libneuralnetworks_common_intermediates/libneuralnetworks_common.a \
 BlobCache:$(_obj)/STATIC_LIBRARIES/libBlobCache_intermediates/libBlobCache.a  \
 nnCache:$(_obj)/STATIC_LIBRARIES/lib_nnCache_intermediates/lib_nnCache.a \
 perfetto_client_experimental:$(_obj)/STATIC_LIBRARIES/libperfetto_client_experimental_intermediates/libperfetto_client_experimental.a \
 protobuf-cpp-lite:$(_obj)/STATIC_LIBRARIES/libprotobuf-cpp-lite_intermediates/libprotobuf-cpp-lite.a \
 perfetto_trace_protos:$(_obj)/STATIC_LIBRARIES/perfetto_trace_protos_intermediates/perfetto_trace_protos.a \
 clang_rt:$(__clang_bindir)../lib64/clang/$(__clang_version)/lib/linux/libclang_rt.builtins-x86_64-android.a

# Unittests dependent on libRScpp_static.a
ifneq (,$(findstring $(THIS_MODULE),$(PVR_UNITTESTS_DEP_LIBRSCPP)))
MODULE_LIBRARY_FLAGS_SUBST += \
 RScpp:$(_obj)/STATIC_LIBRARIES/libRScpp_static_intermediates/libRScpp_static.a
endif
endif # PVR_UNITTESTS_APK

# Always link to specific system libraries.
MODULE_LIBRARY_FLAGS_SUBST += \
  c:$(SYSTEM_LIBRARY_LIBC) \
  m:$(SYSTEM_LIBRARY_LIBM) \
  dl:$(SYSTEM_LIBRARY_LIBDL)

MODULE_INCLUDE_FLAGS := \
 -isystem $(ANDROID_ROOT)/bionic/libc/arch-x86_64/include \
 -isystem $(ANDROID_ROOT)/bionic/libc/kernel/uapi/asm-x86 \
 -isystem $(ANDROID_ROOT)/bionic/libm/include/amd64 \
 $(MODULE_INCLUDE_FLAGS)

MODULE_ARCH_TAG := x86_64

_arch := x86_64
_obj := $(strip $(call path-to-libc-rt,$(_obj),$(_arch)))
ifneq ($(findstring prebuilts,$(_obj)),)
_lib := $(if $(wildcard $(_obj)/lib64),lib64,lib)
else
_lib := lib
endif

else # NDK_ROOT

MODULE_EXE_LDFLAGS += -lc

MODULE_INCLUDE_FLAGS := \
 -isystem $(NDK_SYSROOT)/usr/include/$(CROSS_TRIPLE) \
 $(MODULE_INCLUDE_FLAGS)

MODULE_LIBRARY_FLAGS_SUBST := \
 art:$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/libart.so \
 neuralnetworks_common:$(TARGET_ROOT)/product/$(TARGET_DEVICE)/obj/STATIC_LIBRARIES/libneuralnetworks_common_intermediates/libneuralnetworks_common.a \
 BlobCache:$(TARGET_ROOT)/product/$(TARGET_DEVICE)/obj/STATIC_LIBRARIES/libBlobCache_intermediates/libBlobCache.a  \
 nnCache:$(TARGET_ROOT)/product/$(TARGET_DEVICE)/obj/STATIC_LIBRARIES/lib_nnCache_intermediates/lib_nnCache.a \
 clang_rt:$(NDK_ROOT)/toolchains/llvm/prebuilt/linux-x86_64/lib64/clang/$(__clang_version)/lib/linux/libclang_rt.builtins-x86_64-android.a

# Unittests dependant on libc++_static
ifneq (,$(findstring $(THIS_MODULE),$(PVR_UNITTESTS_APK)))
MODULE_LIBRARY_FLAGS_SUBST := \
 c++_static:$(NDK_ROOT)/out/local/x86_64/libc++_static.a$$(space)$(NDK_ROOT)/out/local/x86_64/libc++abi.a \
 $(MODULE_LIBRARY_FLAGS_SUBST)
endif

ifeq ($(wildcard $(NDK_ROOT)/out/local/x86_64/libc++.so),)
MODULE_LIBRARY_FLAGS_SUBST := \
 c++:$(NDK_ROOT)/sources/cxx-stl/llvm-libc++/libs/x86_64/libc++_static.a$$(space)$(NDK_ROOT)/sources/cxx-stl/llvm-libc++/libs/x86_64/libc++abi.a \
 $(MODULE_LIBRARY_FLAGS_SUBST)
else
MODULE_LIBRARY_FLAGS_SUBST := \
 c++:$(NDK_ROOT)/out/local/x86_64/libc++.so \
 $(MODULE_LIBRARY_FLAGS_SUBST)
MODULE_SYSTEM_LIBRARY_DIR_FLAGS += \
 -Xlinker -rpath-link=$(NDK_ROOT)/out/local/x86_64
endif

ifeq ($(filter-out $(NDK_ROOT)/%,$(NDK_SYSROOT)),)

MODULE_SYSTEM_LIBRARY_DIR_FLAGS += \
 -L$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64 \
 -Xlinker -rpath-link=$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64

# Substitutions performed on MODULE_LIBRARY_FLAGS (NDK workarounds)
MODULE_LIBRARY_FLAGS_SUBST := \
 nativewindow:$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/libnativewindow.so \
 sync:$(TARGET_ROOT)/product/$(TARGET_DEVICE)/system/lib64/libsync.so \
 $(MODULE_LIBRARY_FLAGS_SUBST)

endif # !VNDK

_obj := $(NDK_PLATFORMS_ROOT)/$(TARGET_PLATFORM)/arch-x86_64/usr
_lib := lib64

MODULE_SYSTEM_LIBRARY_DIR_FLAGS := \
 -L$(_obj)/$(_lib) \
 -Xlinker -rpath-link=$(_obj)/$(_lib) \
 $(MODULE_SYSTEM_LIBRARY_DIR_FLAGS)

# Workaround; the VNDK platforms root lacks the crt files
_obj := $(NDK_ROOT)/platforms/$(TARGET_PLATFORM)/arch-x86_64/usr
_lib := lib64
ifeq ($(wildcard $(_obj)),)
_obj := $(NDK_ROOT)/platforms/android-$(API_LEVEL)/arch-x86_64/usr
endif

MODULE_EXE_LDFLAGS := $(MODULE_EXE_LDFLAGS) $(LIBGCC) -Wl,--as-needed -ldl

MODULE_ARCH_TAG := x86_64

endif # NDK_ROOT

MODULE_LIB_LDFLAGS := $(MODULE_EXE_LDFLAGS)

MODULE_LDFLAGS += \
 $(MODULE_SYSTEM_LIBRARY_DIR_FLAGS) \
 $(MODULE_VENDOR_LIBRARY_DIR_FLAGS)

MODULE_EXE_CRTBEGIN := $(_obj)/$(_lib)/crtbegin_dynamic.o
MODULE_EXE_CRTEND := $(call if-exists,\
                     $(_obj)/$(_lib)/crtend_android.o,\
                     $(_obj)/$(_lib)/crtend.o)

MODULE_LIB_CRTBEGIN := $(_obj)/$(_lib)/crtbegin_so.o
MODULE_LIB_CRTEND := $(_obj)/$(_lib)/crtend_so.o

endif # SUPPORT_ANDROID_PLATFORM

ifneq ($(BUILD),debug)
ifeq ($(USE_LTO),1)
MODULE_LDFLAGS := \
 $(sort $(filter-out -W% -D% -isystem /%,$(ALL_CFLAGS) $(ALL_CXXFLAGS))) \
 $(MODULE_LDFLAGS)
endif
endif

MODULE_ARCH_BITNESS := 64
MODULE_ARCH_TYPE := x86

MESON_CROSS_SYSTEM  := linux
MESON_CROSS_CPU_FAMILY := x86_64
MESON_CROSS_CPU :=  x86_64
MESON_CROSS_ENDIAN := little
MESON_CROSS_CROSS_COMPILE := $(CROSS_COMPILE)
MESON_CROSS_CC := $(patsubst @%,%,$(CC))
MESON_CROSS_C_ARGS := -march=x86-64 $(SYS_CFLAGS)
MESON_CROSS_C_LINK_ARGS := $(SYS_LDFLAGS)
MESON_CROSS_CXX := $(patsubst @%,%,$(CXX))
MESON_CROSS_CXX_ARGS := -march=x86-64 $(SYS_CXXFLAGS)
MESON_CROSS_CXX_LINK_ARGS := $(SYS_LDFLAGS)
