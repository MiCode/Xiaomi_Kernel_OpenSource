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

# Basic support option tuning for Android
#
SUPPORT_ANDROID_PLATFORM := 1
SUPPORT_OPENGLES1_V1_ONLY := 1
DONT_USE_SONAMES := 1

# By default, the HAL_VARIANT should match TARGET_DEVICE
#
HAL_VARIANT ?= $(TARGET_DEVICE)

# Always print debugging after 5 seconds of no activity
#
CLIENT_DRIVER_DEFAULT_WAIT_RETRIES ?= 50

# Android WSEGL is always the same
#
OPK_DEFAULT := libpvrANDROID_WSEGL.so

# srvkm is always built
#
KERNEL_COMPONENTS := srvkm

# Make sure all driver memory is zeroed at first use. This is a requirement
# of the Android security model and should not be disabled.
#
PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES ?= 1

# If we are not using the VNDK (it's a full/standalone build) we probably
# have build.prop files, ensuring that the system and vendor directories
# exist under $OUT_DIR. We can use these to detect the partitioning.
#
ifeq ($(VNDK_ROOT),)
 ifeq ($(wildcard $(TARGET_ROOT)/product/$(TARGET_DEVICE)/vendor),)
  PVRSRV_MODULE_BASEDIR := /system/vendor/modules/
  APP_DESTDIR := /data/app
  BIN_DESTDIR := /system/vendor/bin
  FW_DESTDIR := /system/vendor/firmware
 endif
endif

# If these weren't already set up, assume the /vendor partition exists
# and install the DDK accordingly.
#
ifeq ($(PVRSRV_MODULE_BASEDIR)$(APP_DESTDIR)$(BIN_DESTDIR)$(FW_DESTDIR),)
 ifeq ($(is_at_least_nougat),0)
  # Platform versions prior to Nougat do not support installing system apps
  # without Java code (only natives) to /vendor due to a bug in ClassLoader.
  PVR_ANDROID_FORCE_APP_NATIVE_UNPACKED ?= 1
 endif
 PVRSRV_MODULE_BASEDIR := /vendor/modules/
 APP_DESTDIR := /vendor/app
 BIN_DESTDIR := /vendor/bin
 FW_DESTDIR := /vendor/firmware
endif

# Some builds opt into 'compact installs', which avoids polluting the system
# or vendor image with test applications, native executables or debugging
# scripts.
#
SUPPORT_ANDROID_COMPACT_INSTALL ?= 0
ifeq ($(SUPPORT_ANDROID_COMPACT_INSTALL),1)
 APP_DESTDIR := /data/app
 # This will be prepended to $($(THIS_MODULE)_target) by the build system
 TEST_DESTDIR := /data/nativetest
endif

# Enable source fortification by default
#
FORTIFY ?= 1

# Enable compressed debug sections by default
#
COMPRESS_DEBUG_SECTIONS ?= 1

# Enable the memtrack_stats file required to support the Android memtrackhal
#
PVRSRV_ENABLE_MEMTRACK_STATS_FILE := 1

# The <system/window.h> file provides the buffer_handle_t typedef
#
PVR_ANDROID_SYSTEM_WINDOW_HAS_BUFFER_HANDLE_T := 1

# Enable stack trace functions by default. This uses libutils.
#
ifeq ($(SUPPORT_ARC_PLATFORM),)
PVRSRV_NEED_PVR_STACKTRACE_NATIVE ?= 1
 ifeq ($(is_at_least_pie),1)
  PVRSRV_USE_LIBUTILSCALLSTACK ?= 1
 endif
endif

ifeq ($(GTRACE_TOOL),1)
 ifeq ($(is_at_least_pie),1)
  PVRSRV_USE_LIBUTILSCALLSTACK ?= 1
 endif
endif

# If the kernel source tree has a DER formatted version of the
# signing_key.x509 / pem file, in-kernel signature verification
# can be enabled, and the Signer and KeyID fields will be added
# to the signature header.
#
RGX_FW_X509     ?= $(wildcard $(KERNELDIR)/certs/signing_key.x509)
RGX_FW_PRIV_KEY ?= $(wildcard $(KERNELDIR)/certs/signing_key.pem)
ifneq ($(RGX_FW_PRIV_KEY),)
 ifneq ($(RGX_FW_X509),)
  RGX_FW_SIGNED ?= 1
 endif # !RGX_FW_X509
endif # !RGX_FW_PRIV_KEY

# If sanitizer has been enabled, the sanitized binaries must be installed
# under /data/asan on android.
#
ifneq ($(USE_SANITISER),)
 BIN_DESTDIR := /data/asan/vendor/bin
 override SUPPORT_PVR_VALGRIND := 1
endif

##############################################################################
# Unless overridden by the user, assume the RenderScript Compute API level
# matches that of the SDK API_LEVEL.
#
RS_VERSION ?= $(API_LEVEL)
ifneq ($(findstring $(RS_VERSION),21 22),)
RS_VERSION := 20
endif

##############################################################################
# JB MR1 introduces cross-process syncs associated with a fd.
# This requires a new enough kernel version to have the base/sync driver.
#
EGL_EXTENSION_ANDROID_NATIVE_FENCE_SYNC ?= 1
SUPPORT_NATIVE_FENCE_SYNC ?= 1

ifeq ($(PDUMP),1)
override PVR_ANDROID_DEFER_CLEAR := 0
endif

##############################################################################
# Handle various platform cxxflags and includes
#
SYS_CXXFLAGS := -fuse-cxa-atexit $(SYS_CFLAGS)
ifeq ($(SUPPORT_ARC_PLATFORM),)
 SYS_CXXFLAGS += \
  -isystem $(LIBCXX_INCLUDE_PATH) -D_USING_LIBCXX
 ifeq ($(NDK_ROOT),)
  SYS_KHRONOS_INCLUDES := \
   -isystem $(ANDROID_ROOT)/frameworks/native/opengl/include
  ifeq ($(is_at_least_nougat),1)
   SYS_KHRONOS_INCLUDES += \
    -isystem $(ANDROID_ROOT)/frameworks/native/vulkan/include
  endif
 endif
endif

##############################################################################
# Android doesn't use these install script variables. They're still in place
# because the Linux install scripts use them.
#
ifeq ($(SUPPORT_ARC_PLATFORM),)
 SHLIB_DESTDIR := not-used
 EGL_DESTDIR := not-used
endif

##############################################################################
# Must give our EGL/GLES libraries a globally unique name
#
EGL_BASENAME_SUFFIX := _powervr

##############################################################################
# ICS requires that at least one driver EGLConfig advertises the
# EGL_RECORDABLE_ANDROID attribute. The platform requires that surfaces
# rendered with this config can be consumed by an OMX video encoder.
#
EGL_EXTENSION_ANDROID_RECORDABLE := 1

##############################################################################
# ICS added the EGL_ANDROID_blob_cache extension. Enable support for this
# extension in EGL/GLESv2.
#
EGL_EXTENSION_ANDROID_BLOB_CACHE ?= 1

##############################################################################
# Framebuffer target extension is used to find configs compatible with
# the framebuffer
#
EGL_EXTENSION_ANDROID_FRAMEBUFFER_TARGET := 1

##############################################################################
# KitKat added very provisional/early support for sRGB render targets
#
# (Leaving this optional until the framework makes it mandatory.)
#
PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_sRGB ?= 1

##############################################################################
# On Android O, we can't use the Android blob cache from the OpenCL driver,
# because it is not an updatable component yet it depends on libIMGegl.so
# which is updatable. We need to avoid binary conflicts by avoiding direct or
# indirect linkage to libIMGegl.so.
#
# OpenCL uses the Linux implementation of the blob cache on all versions of
# Android to work-around this restriction
OCL_USE_KERNEL_BLOB_CACHE ?= 1

##############################################################################
# When building with the Android NDK at any API_LEVEL, always use the new
# <android/sync.h> header instead of the old location. The NDK header layout
# does not vary depending on API_LEVEL and it uses the modern scheme.
#
ifneq ($(NDK_ROOT),)
PVR_ANDROID_HAS_ANDROID_SYNC_H ?= 1
endif

##############################################################################
# Versions of Android prior to Nougat required Java 7 (OpenJDK).
#
ifeq ($(is_at_least_nougat),0)
 JAVA_VERSION := 7
else
 JAVA_VERSION := 8
endif

# Versions of Android prior to Nougat required .apk files to be processed with
# zipalign. Using this tool on Nougat or greater will corrupt the .apk file,
# as alignment is already done by signapk.jar, so we must disable it.
#
ifeq ($(is_at_least_nougat),0)
LEGACY_USE_ZIPALIGN ?= 1
endif

##############################################################################
# Marshmallow needs --soname turned on
#
ifeq ($(is_at_least_marshmallow),1)
PVR_ANDROID_NEEDS_SONAME ?= 1
endif

##############################################################################
# Marshmallow replaces RAW_SENSOR with RAW10, RAW12 and RAW16
#
ifeq ($(is_at_least_marshmallow),1)
PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_RAWxx := 1
endif

##############################################################################
# Marshmallow has redesigned sRGB support
#
ifeq ($(is_at_least_marshmallow),1)
PVR_ANDROID_HAS_SET_BUFFERS_DATASPACE ?= 1
endif

##############################################################################
# Marshmallow has partial updates support
#
ifeq ($(is_at_least_marshmallow),0)
EGL_EXTENSION_PARTIAL_UPDATES := 0
endif

##############################################################################
# fenv was rewritten in Marshmallow
#
ifeq ($(is_at_least_marshmallow),1)
PVR_ANDROID_HAS_WORKING_FESETROUND := 1
endif

##############################################################################
# Marshmallow added the 4:2:2 and 4:4:4 flexible YUV formats
#
ifeq ($(is_at_least_marshmallow),1)
PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_YCbCr_422_444 := 1
endif

##############################################################################
# Nougat has a substantially redesigned android_dataspace_t enum
#
ifeq ($(is_at_least_nougat),1)
PVR_ANDROID_HAS_SET_BUFFERS_DATASPACE_2 := 1
endif

##############################################################################
# Nougat advertises and utilizes the EGL_KHR_mutable_render_buffer extension
#
ifeq ($(is_at_least_nougat),1)
EGL_EXTENSION_MUTABLE_RENDER_BUFFER := 1
endif

##############################################################################
# Nougat has a new 'set_shared_buffer_mode' perform() function on the
# ANativeWindow object
#
ifeq ($(is_at_least_nougat),1)
PVR_ANDROID_HAS_SHARED_BUFFER := 1
endif

##############################################################################
# Starting with Nougat the debugger uses the binder interface
#
ifeq ($(is_at_least_nougat),1)
PVR_ANDROID_DEBUGGER_USE_BINDER := 1
endif

##############################################################################
# Nougat supports the Vulkan graphics API
#
# We would prefer to track the glslc program from Android AOSP, because it's
# the same one used in the NDK, and we should control where the sources come
# from. Unfortunately, the upstream build doesn't create a 'glslc' standalone,
# and it's not a standardised tool for most Linux distros yet, so we build one
# in the IMG customisation of Android. If the tool isn't present in the
# build's host output bin directory, we'll fall back to a system copy, and if
# that does not exist either, we'll make it the empty string, so the affected
# tests can be disabled..
#
ifeq ($(is_at_least_nougat),1)
 ifeq ($(GLSLC),)
  ifeq ($(NDK_ROOT),)
   GLSLC ?= $(OUT_DIR)/host/$(HOST_OS)-$(HOST_PREBUILT_ARCH)/bin/glslc
  else
   GLSLC ?= $(NDK_ROOT)/shader-tools/$(HOST_OS)-$(HOST_ARCH)/glslc
  endif
  ifeq ($(wildcard $(GLSLC)),)
   GLSLC := $(shell $(SHELL) -c "command -v glslc")
   ifeq ($(GLSLC),)
    $(warning glslc could not be found.)
   endif
  endif
 endif
endif

##############################################################################
# Nougat advertises the HAL_PIXEL_FORMAT_FLEX_RGB{,A} pixel formats
#
ifeq ($(is_at_least_nougat),1)
PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_FLEX := 1
endif

##############################################################################
# Nougat changed the behaviour of core Surface class functions
#
ifeq ($(is_at_least_nougat),1)
PVR_ANDROID_QUEUE_CANCEL_BUFFER_CLOSES_FENCE_ON_ERROR := 1
endif

##############################################################################
# Pie supports hwcomposer v2
#
ifeq ($(is_at_least_pie),1)
PVR_ANDROID_HAS_HWCOMPOSER_2 ?= 1
endif

##############################################################################
# Nougat MR1 supports the new gralloc v1 API
#
ifeq ($(is_at_least_nougat_mr1),1)
PVR_ANDROID_HAS_GRALLOC_1 ?= 1
# Set GRALLOC_USAGE_PRIVATE_3 as default if we're in gralloc v0.
# It only can be one of GRALLOC_USAGE_PRIVATE_{0->3}.
#
ifneq ($(PVR_ANDROID_HAS_GRALLOC_1),1)
 PVR_ANDROID_GRALLOC_USAGE_REQUEST_TWIDDLE_MEMORY_LAYOUT ?= (1ULL << 31)
endif
endif

# On Android O, we can't use EGL sharing from the OpenCL driver, because
# it is not an updatable component yet it depends on libIMGegl.so which is
# updatable. We need to avoid binary conflicts by avoiding direct or indirect
# linkage to libIMGegl.so.
#
ifeq ($(is_at_least_oreo),1)
OCL_USE_EGL_SHARING ?= 0
OCL_USE_GRALLOC_IMAGE_SHARING ?= 1
endif

# On Android O, we want to insulate the OpenCL library more from other DDK
# components, so it can remain non-updated and the other parts can be updated
# from the Play Store. We therefore need to eliminate its dependency on the
# runtime loadable version of the uniflex writer.
#
ifeq ($(is_at_least_oreo),1)
OCL_ONLINE_COMPILER_DIRECTLY_LINKED ?= 1
endif

# OpenCL-compiled precompiled headers can be enabled or disabled
# based on library size constraints.
#
OCLC_ENABLE_PCH ?= 1

# On Android O, link the pvrANDROID_WSEGL module directly into IMGEGL. This
# cuts down on unnecessary dynamic library dependencies.
#
#ifeq ($(is_at_least_oreo),1)
#EGL_WSEGL_DIRECTLY_LINKED ?= 1
#endif

# On Android O, gralloc1 must advertise the 'layered buffers' capability.
#
ifeq ($(is_at_least_oreo),1)
PVR_ANDROID_HAS_GRALLOC1_CAPABILITY_LAYERED_BUFFERS ?= 1
endif

# On Android O, gralloc1 RELEASE implies object deletion and this must be
# advertised to the framework. On N MR1, gralloc1 continues to use the old
# RELEASE behaviour. This capability is strongly recommended and will become
# mandatory in future releases.
#
ifeq ($(is_at_least_oreo),1)
override PVR_ANDROID_HAS_GRALLOC1_CAPABILITY_RELEASE_IMPLY_DELETE := 1
endif

# On Android O, new pixel formats must be supported by gralloc.
#
ifeq ($(is_at_least_oreo),1)
PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_RGBA_1010102 ?= 1
PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_RGBA_FP16 ?= 1
# If FP16 is supported, then EGL_EXTENSION_PIXEL_FORMAT_FLOAT must be defined.
ifeq ($(PVR_ANDROID_HAS_HAL_PIXEL_FORMAT_RGBA_FP16),1)
override EGL_EXTENSION_PIXEL_FORMAT_FLOAT := 1
endif
endif

# On Android O, composition timings can be obtained directly from the
# platform without workarounds. Use the new method on O.
ifeq ($(is_at_least_oreo),1)
PVR_ANDROID_HAS_COMPOSITION_TIMINGS ?= 1
endif

# On Android O, there is a new hardware_buffer interface that the DDK must
# use. This is provided by the VNDK.
#
ifeq ($(is_at_least_oreo),1)
override PVR_ANDROID_HAS_HARDWARE_BUFFER := 1
endif

# On Android O, the <sync/sync.h> file was moved to <android/sync.h> for
# DDK use. A symlink was left for legacy reasons, but it conflicts with
# the NDK. Tell the driver to avoid using the symlink compatibility.
#
ifeq ($(is_at_least_oreo),1)
override PVR_ANDROID_HAS_ANDROID_SYNC_H := 1
endif

# On Android O, a new native window query has been added to test a window
# for validity. This is called by apps and the framework so it must be
# supported in our 'testwrap' framework.
# NOTE: This feature has not been reconciled into AOSP master.
#
ifeq ($(is_at_least_oreo),1)
PVR_ANDROID_HAS_NATIVE_WINDOW_IS_VALID ?= 1
endif

# From Android O mr1, use the HIDL interface.
ifeq ($(is_at_least_oreo_mr1),1)
PVR_DEBUGGER_SUPPORTS_HIDL_API ?= 1
endif

# Enabling IPC based mechanism for apphints (requires Treble)
ifeq ($(is_at_least_oreo_mr1),1)
ifeq ($(MTK_MINI_PORTING),1)
PVR_APPHINTS_IPC := 0
else
PVR_APPHINTS_IPC ?= 1
endif
endif

# If this is newer than oreo-mr1, <system/window.h> provides the typedef for
# buffer_handle_t. Otherwise, it is provided by <cutils/native_handle.h>.
# Handle this build issue here.
#
ifeq ($(is_at_least_oreo_mr1),1)
override PVR_ANDROID_SYSTEM_WINDOW_HAS_BUFFER_HANDLE_T := 0
endif

# Starting with oreo-mr1 (full treble), Android places strict restrictions on
# the system libraries (called LL-NDK) a vendor can link against. Making a
# list, to prevent DDK developers from unknowingly linking against incorrect
# libraries. Read the VNDK documentation to understand the details.
#
ifeq ($(is_at_least_oreo_mr1),1)
PVR_ANDROID_LLNDK_LIBRARIES := \
 log RS EGL sync GLESv1_CM GLESv2 GLESv3 vndksupport nativewindow
endif

# Some unittests within the DDK are dependent on libraries that android doesn't
# allow vendor libraries to link against. Unittests are independent executables
# so adding them to list of exceptions.
#
ifeq ($(is_at_least_oreo_mr1),1)
PVR_UNITTESTS_APK := \
 eglinfo_android gles1test1_android gles2test1_android gles3test1_android \
 gles3yuvtest1_android hal_blit_test rsc_unit_test tearing_test_android \
 vkbonjour_android yuvtest_android
PVR_UNITTESTS_DEP_LIBRSCPP := rsc_unit_test2
endif

# On Android P, mapper 2.1 has been supported
#
ifeq ($(is_at_least_pie),1)
PVR_ANDROID_HAS_MAPPER_VERSION_2_1 ?= 1
endif

# Colorspace support on Android
#
ifeq ($(is_at_least_pie),1)
PVR_ANDROID_HAS_HAL_DATASPACE ?= 1
endif

ifeq ($(PVR_ANDROID_HAS_HAL_DATASPACE),1)
# If colorspace functionalities are in use, then EGL extensions
# EGL_EXTENSION_KHR_GL_COLORSPACE must be defined
override EGL_EXTENSION_KHR_GL_COLORSPACE := 1
PVR_ANDROID_HAS_HAL_DATASPACE_SCRGB ?= 1
PVR_ANDROID_HAS_HAL_DATASPACE_DISPLAY_P3 ?= 1
PVR_ANDROID_HAS_HAL_DATASPACE_BT2020 ?= 1
endif

# In Android Pie, a new native window query has been added to test a window
# for maximum buffer counts so that we can know if a window has only one
# buffer or not.
#
ifeq ($(is_at_least_pie),1)
PVR_ANDROID_HAS_NATIVE_WINDOW_MAX_BUFFER_COUNT := 1
endif

# Support Common 1.1 hal since Pie
#
ifeq ($(is_at_least_pie),1)
PVR_ANDROID_HAS_COMMON_1_1 := 1
endif

##############################################################################
# Pie supports hwcomposer v2.2
#
ifeq ($(is_at_least_pie),1)
PVR_ANDROID_HAS_HWCOMPOSER_2_2 ?= 1

# Enable support of metadata feature in graphicshal. This feature serves Vulkan
# extensions on Android Pie and Android 10. It also will be one of key features
# in Gralloc 4.
#
PVR_ANDROID_HAS_GRALLOC_METADATA ?= 1
endif

##############################################################################
# Android-10 (Q) supporting features
#
ifeq ($(is_at_least_q), 1)
# Expose EGL_EXT_client_extensions
#
EGL_EXTENSION_CLIENT_EXTENSIONS := 1

# Expose EGL_EXT_platform_base
#
EGL_EXTENSION_PLATFORM_BASE := 1

# Expose EGL_KHR_platform_android
#
EGL_EXTENSION_KHR_PLATFORM_ANDROID := 1

# Expose EGL_EXT_gl_colorspace_display_p3_passthrough
#
EGL_EXTENSION_KHR_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH := 1

# sync_fence_info_data and sync_pt_info are deprecated. Use modern sync APIS
# e.g: sync_file_info
#
override PVR_USE_LEGACY_SYNC_H := 0

# The compiler search paths are out-of-order for finding stddef.h in the new
# toolchain. The built-in header directory is invoked before searching
# $(LIBCXX_INCLUDE_PATH) so that including the next stddef.h is unsuccessful.
# Workaround the issue with including build-in headers directory after
# $(LIBCXX_INCLUDE_PATH).
#
BUILDIN_HEADERS := $(shell $(CC) -E - -v 2>&1 < /dev/null | grep clang | \
                     tail -1)
SYS_CXXFLAGS += -isystem $(BUILDIN_HEADERS)

# Atrace hal has been supported from Q.
#
PVR_ANDROID_HAS_ATRACE_HAL := 1

# Support Mapper 3.0 hal since Q
#
PVR_ANDROID_HAS_MAPPER_3 := 1

# Support Common 1.2 hal since Q
#
PVR_ANDROID_HAS_COMMON_1_2 := 1

# Support Safe Union 1.0 hal since Q
#
PVR_ANDROID_HAS_SAFE_UNION_1_0 := 1

# Replace the legacy data spaces with modern data spaces that will be
# expected in surfaceflinger and hardware composer in Android Q (10.0).
#
PVR_ANDROID_HAS_HAL_DATASPACE_V0 := 1

# Use LLVM linker in Android Q (10.0).
#
USE_LLD := 1

# Use Link Time Optimisation by default since Android Q (10.0).
#
ifneq ($(BUILD),debug)
USE_LTO ?= 1
endif

# Enable PVR_DPF and Android logcat in debug build
#
ifeq ($(BUILD),debug)
PVRSRV_NEED_PVR_DPF ?= 1
endif

# Enable PVR ION memory stats by default.
#
PVRSRV_ENABLE_PVR_ION_STATS ?= 1

endif

##############################################################################
# Android-11 (R) supporting features
#
ifeq ($(is_at_least_r), 1)
# Support Mapper 4.0 hal since R
#
PVR_ANDROID_HAS_MAPPER_4 := 1

# Enable perfetto producer
#
PVR_ANDROID_HAS_PERFETTO := 1

# Google removed GCC from Android R prebuilts. Use clang for kbuild.
# Using set of LLVM tools by default. But they could be commented out
# depending on platforms.
#
override KERNEL_CC := clang
KERNEL_LD := ld.lld
KERNEL_NM := llvm-nm
KERNEL_OBJCOPY := llvm-objcopy

# CCACHE doesn't work with clang for kernel build.
# Disable for now
#
override USE_CCACHE := 0

JAVA_VERSION := 11

PVR_ANDROID_HAS_GRAPHICSHAL_HIDL ?= 0
ifeq ($(PVR_ANDROID_HAS_GRAPHICSHAL_HIDL),1)
 override PVR_ANDROID_HAS_GRALLOC_4 := 1
endif

# Support gralloc v4 by default on Android R.
#
PVR_ANDROID_HAS_GRALLOC_4 ?= 1
ifeq ($(PVR_ANDROID_HAS_GRALLOC_4),1)
  override PVR_ANDROID_HAS_GRALLOC_METADATA := 1
endif

# Support EGL 1.5 on Android 11.
#
IMGEGL_PLATFORM_ANDROID_HAS_1_5 ?=1

endif

##############################################################################
# Placeholder for future version handling
#
ifeq ($(is_future_version),1)
-include ../common/android/future_version.mk
endif

# Do not use framebuffer device
#
ifeq ($(SUPPORT_KMS),1)
 SUPPORT_DRM_FBDEV_EMULATION ?= 0
endif

# Append "32" suffix to the 32-bit binaries name when building with MULTIARCH=1
#
# This is beneficial because all of the binaries are installed to /vendor/bin
# and there is a name collision between 64-bit and 32-bit binaries.
#
MULTIARCH_32BIT_EXEC_SUFFIX ?= 1
