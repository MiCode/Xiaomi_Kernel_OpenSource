########################################################################### ###
#@File
#@Title         Root build configuration.
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

ifneq ($(INTERNAL_CLOBBER_ONLY),true)
 ifeq ($(RGX_BVNC),)
  ifeq ($(NO_HARDWARE),1)
   $(error Error: Must specify RGX_BVNC when building for NO_HARDWARE)
  else ifeq ($(PVR_ARCH),)
   $(error Error: Must specify PVR_ARCH when doing a kernel mode build without RGX_BVNC set)
  endif
 endif
endif

# If building for volcanic architecture use the core_volcanic.mk instead
ifeq ($(PVR_ARCH),volcanic)
  include ../config/core_volcanic.mk
else

# Configuration wrapper for new build system. This file deals with
# configuration of the build. Add to this file anything that deals
# with switching driver options on/off and altering the defines or
# objects the build uses.
#
# At the end of this file is an exhaustive list of all variables
# that are passed between the platform/config stage and the generic
# build. PLEASE refrain from adding more variables than necessary
# to this stage -- almost all options can go through config.h.
#

# Sanity check: Make sure preconfig has been included
ifeq ($(TOP),)
$(error TOP not defined: Was preconfig.mk included in root makefile?)
endif

################################# MACROS ####################################

ALL_TUNABLE_OPTIONS :=

# This records the config option's help text and default value. Note that
# the help text can't contain a literal comma. Use $(comma) instead.
define RegisterOptionHelp
ALL_TUNABLE_OPTIONS += $(1)
ifeq ($(INTERNAL_DESCRIPTION_FOR_$(1)),)
INTERNAL_DESCRIPTION_FOR_$(1) := $(3)
endif
INTERNAL_CONFIG_DEFAULT_FOR_$(1) := $(2)
$(if $(4),\
	$(error Too many arguments in config option '$(1)' (stray comma in help text?)))
endef


# Write out a GNU make option for both user & kernel
#
define BothConfigMake
$$(eval $$(call KernelConfigMake,$(1),$(2)))
$$(eval $$(call UserConfigMake,$(1),$(2)))
endef

# Conditionally write out a GNU make option for both user & kernel
#
define TunableBothConfigMake
$$(eval $$(call _TunableKernelConfigMake,$(1),$(2)))
$$(eval $$(call _TunableUserConfigMake,$(1),$(2)))
$(call RegisterOptionHelp,$(1),$(2),$(3),$(4))
endef

# Write out an option for both user & kernel
#
define BothConfigC
$$(eval $$(call KernelConfigC,$(1),$(2)))
$$(eval $$(call UserConfigC,$(1),$(2)))
endef

# Conditionally write out an option for both user & kernel
#
define TunableBothConfigC
$$(eval $$(call _TunableKernelConfigC,$(1),$(2)))
$$(eval $$(call _TunableUserConfigC,$(1),$(2)))
$(call RegisterOptionHelp,$(1),$(2),$(3),$(4))
endef

# Use this to mark config options which have to exist, but aren't
# user-tunable. Warn if an attempt is made to change it.
#
define NonTunableOption
$(if $(filter command line environment,$(origin $(1))),\
	$(error Changing '$(1)' is not supported))
endef

############################### END MACROS ##################################

# Check we have a new enough version of GNU make.
#
need := 3.81
ifeq ($(filter $(need),$(firstword $(sort $(MAKE_VERSION) $(need)))),)
$(error A version of GNU make >= $(need) is required - this is version $(MAKE_VERSION))
endif

include ../defs.mk

# Infer PVR_BUILD_DIR from the directory configuration is launched from.
# Check anyway that such a directory exists.
#
PVR_BUILD_DIR := $(notdir $(abspath .))
$(call directory-must-exist,$(TOP)/build/linux/$(PVR_BUILD_DIR))

# Output directory for configuration, object code,
# final programs/libraries, and install/rc scripts.
#
BUILD        ?= release
ifneq ($(filter $(WINDOW_SYSTEM),xorg wayland nullws nulldrmws screen surfaceless lws-generic),)
OUT          ?= $(TOP)/binary_$(PVR_BUILD_DIR)_$(WINDOW_SYSTEM)_$(BUILD)
else
OUT          ?= $(TOP)/binary_$(PVR_BUILD_DIR)_$(BUILD)
endif

# Use abspath, which doesn't require the path to already exist, to remove '.'
# and '..' path components. This allows paths to be manipulated without things
# ending up in the wrong place.
override OUT := $(abspath $(if $(filter /%,$(OUT)),$(OUT),$(TOP)/$(OUT)))

CONFIG_MK			:= $(OUT)/config.mk
CONFIG_H			:= $(OUT)/config.h
CONFIG_KERNEL_MK	:= $(OUT)/config_kernel.mk
CONFIG_KERNEL_H		:= $(OUT)/config_kernel.h

# Convert commas to spaces in $(D). This is so you can say "make
# D=config-changes,freeze-config" and have $(filter config-changes,$(D))
# still work.
override D := $(subst $(comma),$(space),$(D))

# Create the OUT directory
#
$(shell mkdir -p $(OUT))

# For a clobber-only build, we shouldn't regenerate any config files
ifneq ($(INTERNAL_CLOBBER_ONLY),true)

# Core handling
#

-include ../config/user-defs.mk
-include ../config/kernel-defs.mk

# Disabling the online OpenCL compiler breaks the OpenCL spec.
# Use this option carefully (i.e. for embedded usage only).
OCL_ONLINE_COMPILATION ?= 1

# Some platforms don't have blob cache support, or the blob cache isn't usable
# for some reason. Make it possible to disable the OpenCL driver's use of it.
OCL_USE_KERNEL_BLOB_CACHE ?= 1

# Allow OpenCL to disable image sharing with EGL on platforms that don't support it.
OCL_USE_EGL_SHARING ?= 1
OCL_USE_GRALLOC_IMAGE_SHARING ?= 0

# Rather than requiring the user to have to define two variables (one quoted,
# one not), make PVRSRV_MODNAME a non-tunable and give it an overridable
# default here.
#
PVRSRV_MODNAME := pvrsrvkm
PVRSYNC_MODNAME := pvr_sync

# Normally builds don't touch these, but we use them to influence the
# components list. Make sure these are defined early enough to make this
# possible.
#

# Skip defining these UM/FW macros for kbuilds which do not define RGX_BVNC
ifneq ($(RGX_BNC_CONFIG_KM_HEADER),)
# Only the Firmware needs this make macro.
SUPPORT_META_DMA :=\
 $(shell grep -qw RGX_FEATURE_META_DMA $(RGX_BNC_CONFIG_KM) && echo 1)

# Only the Firmware needs this make macro.
SUPPORT_META_COREMEM :=\
 $(shell grep -qe 'RGX_FEATURE_META_COREMEM_SIZE ([123456789][1234567890]*U*)' $(RGX_BNC_CONFIG_KM) && echo 1)

# Client drivers, firmware and libsrv_um need this make macro.
SUPPORT_COMPUTE := \
 $(shell grep -qw RGX_FEATURE_COMPUTE $(RGX_BNC_CONFIG_KM) && echo 1)

# FASTRENDER_DM | COMPUTE_ONLY | SUPPORT_TQ
# -----------------------------------------
# 0             | 0            | 1
# 1             | 0            | 1
# 0             | 1            | 0
# 1             | 1            | 1
SUPPORT_TQ := \
 $(shell grep -qw RGX_FEATURE_FASTRENDER_DM $(RGX_BNC_CONFIG_KM) || ! grep -qw RGX_FEATURE_COMPUTE_ONLY $(RGX_BNC_CONFIG_KM) && echo 1)

# Macro used by client driver makefiles only.
OPENCL_CDM_FORMAT_2 ?= \
 $(shell grep -qw "RGX_FEATURE_CDM_CONTROL_STREAM_FORMAT (2U*)" $(RGX_BNC_CONFIG_KM) && echo 1)

# Only the Firmware needs this make macro.
SUPPORT_MIPS_FIRMWARE :=\
 $(shell grep -qw RGX_FEATURE_MIPS $(RGX_BNC_CONFIG_KM) && echo 1)

# Only the Firmware needs this make macro.
SUPPORT_RISCV_FIRMWARE :=\
 $(shell grep -qw RGX_FEATURE_RISCV_FW_PROCESSOR $(RGX_BNC_CONFIG_KM) && echo 1)

# Firmware and libsrv_um need this make macro.
SUPPORT_TLA :=\
 $(shell grep -qw RGX_FEATURE_TLA $(RGX_BNC_CONFIG_KM) && echo 1)

# Firmware and libsrv_um need this make macro.
SUPPORT_FASTRENDER_DM :=\
 $(shell grep -qw RGX_FEATURE_FASTRENDER_DM $(RGX_BNC_CONFIG_KM) && echo 1)

# Firmware and libsrv_um need this make macro.
SUPPORT_SIGNAL_FILTER := \
 $(shell grep -qw RGX_FEATURE_SIGNAL_SNOOPING $(RGX_BNC_CONFIG_KM) && echo 1)

# Compute only used by Firmware and client drivers
SUPPORT_COMPUTE_ONLY := \
 $(shell grep -qw RGX_FEATURE_COMPUTE_ONLY $(RGX_BNC_CONFIG_KM) && echo 1)

# Macro used by client driver makefiles only.
ifneq ($(wildcard $(RGX_BNC_CONFIG_H)),)
 SUPPORT_ES32 :=\
    $(shell grep -qw RGX_FEATURE_ASTC $(RGX_BNC_CONFIG_H) && grep -qw RGX_FEATURE_GS_RTA_SUPPORT $(RGX_BNC_CONFIG_KM) && echo 1)
    
 SUPPORT_OPENGL := \
    $(shell grep -qw RGX_FEATURE_BC1_TO_5 $(RGX_BNC_CONFIG_H) && echo 1)
endif
endif

PVRSRV_VZ_NUM_OSID ?= 1

ifeq ($(SUPPORT_COMPUTE_ONLY),1)
	# A compute only core has neither TLA or 3D
	PVRSRV_SUPPORT_LEGACY_TQ_UM ?= 0
	PVRSRV_SUPPORT_LEGACY_TQ_FW ?= 0
else
	ifneq ($(SUPPORT_FASTRENDER_DM),1)
	# Without a TDM, the TLA and/or 3D TQ is required
	PVRSRV_SUPPORT_LEGACY_TQ_UM ?= 1
	PVRSRV_SUPPORT_LEGACY_TQ_FW ?= 1
	endif
endif

# Default place for binaries and shared libraries
BIN_DESTDIR ?= /usr/local/bin
INCLUDE_DESTDIR ?= /usr/include
SHARE_DESTDIR ?= /usr/local/share
SHLIB_DESTDIR ?= /usr/lib
FW_DESTDIR ?= /lib/firmware

# Determine use of safety critical implementation of IMGEGL

# Build's selected list of components.
# - components.mk is a per-build file that specifies the components that are
#   to be built
ifeq ($(SC_EGL),1)
 # Include safety critical DDK components
 -include components_sc.mk
 override SERVICES_SC ?= 1
else
 # Include consumer DDK components
 -include components.mk
endif

ifeq ($(_CLANG),true)
 ifneq ($(SUPPORT_BUILD_LWS),)
  ifneq ($(filter command line environment,$(origin SUPPORT_BUILD_LWS)),)
   $(error The SUPPORT_BUILD_LWS build option is not supported with the Clang compiler. Please use GCC instead.)
  else ifneq ($(filter command line environment,$(origin MESA_EGL)),)
   $(error The MESA_EGL build option is not supported with the Clang compiler. Please use GCC instead.)
  else
   $(error The '$(WINDOW_SYSTEM)' window system does not support being built with the Clang compiler. Please use GCC instead.)
  endif
 endif
endif

# Safety Critical Services
$(eval $(call TunableBothConfigMake,SERVICES_SC,$(SERVICES_SC)))

ifeq ($(SERVICES_SC),1)
$(eval $(call TunableBothConfigC,SUPPORT_FREELIST_GROW,))
endif

# Set up the host and target compiler.
include ../config/compiler.mk

# pvr-gdb needs extra components
#
ifeq ($(SUPPORT_DEBUGGER),1)
 ifneq ($(filter opencl,$(COMPONENTS)),)
  COMPONENTS += libpvrdebugger pvrgtrace
 endif
 ifneq ($(filter opengles3,$(COMPONENTS)),)
  COMPONENTS += libpvrdebugger pvrgtrace
 endif
 ifneq ($(filter vulkan,$(COMPONENTS)),)
  COMPONENTS += libpvrdebugger pvrgtrace
 endif
 ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
  COMPONENTS += libpvrdebugipc
 endif
 ifeq ($(SUPPORT_DEBUGGER_UNITTESTS), 1)
  COMPONENTS += gdb_ocl_test gdb_unit_test dbg_pipe_test dbg_unit_test
 endif
endif

$(eval $(call BothConfigMake,PVR_ARCH,$(PVR_ARCH)))

ifneq ($(SUPPORT_BUILD_LWS),)
 ifneq ($(SYSROOT),)
  $(warning ******************************************************)
  $(warning WARNING: You have specified a SYSROOT, or are using a)
  $(warning buildroot compiler, and enabled SUPPORT_BUILD_LWS. We)
  $(warning will ignore the sysroot and will build all required)
  $(warning LWS components. Unset SUPPORT_BUILD_LWS if this is not)
  $(warning what you want.)
  $(warning ******************************************************)
 endif

 ifneq ($(origin SUPPORT_BUILD_LWS),file)
  $(warning ******************************************************)
  $(warning WARNING: Enabling SUPPORT_BUILD_LWS is deprecated.)
  ifneq ($(filter surfaceless wayland xorg,$(WINDOW_SYSTEM)),)
   $(warning You should not need to set this explicitly.)
  else
   $(warning You should be setting SYSROOT instead, which is)
   $(warning documented in the Rogue DDK Linux and Rogue DDK)
   $(warning Linux WS Platform Guides.)
  endif
  $(warning ******************************************************)
 endif

 override SYSROOT :=
endif


ifneq ($(strip $(LWS_PREFIX)),)
endif

# The name of the file that contains the set of tarballs that should be
# built to support a given linux distribution
LWS_DIST ?= tarballs-ubuntu-next

ifeq ($(SUPPORT_BUILD_LWS),1)
 COMPONENTS += ${LWS_COMPONENTS}
endif

$(if $(filter config,$(D)),$(info Build configuration:))

################################# CONFIG ####################################

-include ../config/core-internal.mk

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_FIRMWARE_HEAP_POLICY,5,\
Apphint that configures the Firmware Heap mode. First two bits configures_\
the free block selection policy. Bit 2 configures the bucket selection policy_\
))


$(eval $(call TunableKernelConfigMake,SUPPORT_LINUX_WRAP_EXTMEM_PAGE_TABLE_WALK,))
$(eval $(call TunableKernelConfigC,SUPPORT_LINUX_WRAP_EXTMEM_PAGE_TABLE_WALK,,\
This allows the kernel wrap memory handler to determine the pages_\
associated with a given virtual address by performing a walk-through of the corresponding_\
page tables. This method is only used with virtual address regions that belong to device_\
or with virtual memory regions that have VM_IO set._\
This setting is for Linux platforms only ._\
))

# Firmware toolchain versions
$(eval $(call BothConfigMake,METAG_VERSION_NEEDED,2.8.1.0.3))
$(eval $(call BothConfigMake,MIPS_VERSION_NEEDED,2014.07-1))
$(eval $(call BothConfigMake,RISCV_VERSION_NEEDED,1.0.1))

ifneq ($(SUPPORT_NEUTRINO_PLATFORM), 1)

# If KERNELDIR is set, write it out to the config.mk, with
# KERNEL_COMPONENTS and KERNEL_ID
#
ifneq ($(strip $(KERNELDIR)),)
PVRSRV_MODULE_BASEDIR ?= /lib/modules/$(KERNEL_ID)/extra/
$(eval $(call BothConfigMake,KERNELDIR,$(KERNELDIR)))
$(eval $(call BothConfigMake,KERNEL_ID,$(KERNEL_ID)))
$(eval $(call BothConfigMake,PVRSRV_MODULE_BASEDIR,$(PVRSRV_MODULE_BASEDIR)))
$(eval $(call BothConfigMake,KERNEL_COMPONENTS,$(KERNEL_COMPONENTS)))
$(eval $(call TunableKernelConfigMake,EXTRA_PVRSRVKM_COMPONENTS,,\
List of components that should be built in to pvrsrvkm.ko$(comma) rather than_\
forming separate kernel modules._\
))

# If KERNEL_CROSS_COMPILE is set to "undef", this is magically
# equivalent to being unset. If it is unset, we use CROSS_COMPILE
# (which might also be unset). If it is set, use it directly.
ifneq ($(KERNEL_CROSS_COMPILE),undef)
KERNEL_CROSS_COMPILE ?= $(CROSS_COMPILE)
$(eval $(call TunableBothConfigMake,KERNEL_CROSS_COMPILE,))
endif

# Alternatively, allow the CC used for kbuild to be overridden
# exactly, bypassing any KERNEL_CROSS_COMPILE configuration.
# LD, NM and OBJCOPY could be overridden by set of CC tools.
$(eval $(call TunableBothConfigMake,KERNEL_CC,))
$(eval $(call TunableBothConfigMake,KERNEL_LD,))
$(eval $(call TunableBothConfigMake,KERNEL_NM,))
$(eval $(call TunableBothConfigMake,KERNEL_OBJCOPY,))

# Check the KERNELDIR has a kernel built and also check that it is
# not 64-bit, which we do not support.
KERNEL_AUTOCONF := \
 $(strip $(wildcard $(KERNELDIR)/include/linux/autoconf.h) \
         $(wildcard $(KERNELDIR)/include/generated/autoconf.h))
ifeq ($(KERNEL_AUTOCONF),)
$(warning autoconf.h not found in $$(KERNELDIR)/include/linux \
or $$(KERNELDIR)/include/generated. Check your $$(KERNELDIR) variable \
and kernel configuration.)
endif
else
$(if $(KERNEL_COMPONENTS),$(warning KERNELDIR is not set. Kernel components cannot be built))
endif

# Platforms can make use of the ChromiumOS upstream kernels. Make the build
# system aware of which features are available within those kernels.
-include ../common/chromiumos_kernel.mk

# Enable Client CCB grow
PVRSRV_ENABLE_CCCB_GROW ?= 1
$(eval $(call TunableBothConfigC,PVRSRV_ENABLE_CCCB_GROW,,\
This controls the feature that allows the Services client CCBs to grow_\
when they become full._\
))

endif # !Neutrino


ifneq ($(_window_systems),)
# If there's only one window system then don't output this option as part
# of `make confighelp`
ifeq ($(words $(_window_systems)),1)
$(eval $(call BothConfigMake,WINDOW_SYSTEM,$(WINDOW_SYSTEM)))
else
$(eval $(call TunableBothConfigMake,WINDOW_SYSTEM,,\
Window system to use ($(_window_systems))._\
))
endif
endif

# Ideally configured by platform Makefiles, as necessary
#
SHADER_DESTDIR := $(SHARE_DESTDIR)/pvr/shaders/

# RGX_FW_SIGNED=1 means that the user wants to sign the firmware binaries.
# In such a case, the build system needs to ensure that it has access to the
# public / private key and the message digest algo is correctly specified.
# If unavailable, we must handle these problems gracefully (by falling back
# to known values) or reporting errors to the user.
# Here's what we do,
# -> If kernel version is less than 4.9, we throw a warning as signing feature
#    is unsupported for older kernels, and fall back to unsigned firmware.
# -> If kernel version is >= 4.9,
#    .. If message digest algo is not specified, we fall back to "sha1"
#    .. If public / private key are not specified, we throw an error and exit.
ifeq ($(RGX_FW_SIGNED),1)
 ifeq ($(call kernel-version-at-least,4,9),true)
  override RGX_FW_HASH ?= sha1
  ifeq ($(RGX_FW_PRIV_KEY),)
    $(error RGX_FW_PRIV_KEY must be set for RGX_FW_SIGNED=1.)
  endif # !RGX_FW_PRIV_KEY
  ifeq ($(RGX_FW_X509),)
    $(error RGX_FW_X509 must be set for RGX_FW_SIGNED=1.)
  endif # !RGX_FW_X509
 else
  $(warning Firmware signing is not implemented for kernels < 4.9. Signing will not be done!!)
  override RGX_FW_SIGNED := 0
 endif # ($(call kernel-version-at-least,4,9),true)
else  # RGX_FW_SIGNED
endif # RGX_FW_SIGNED


ifeq ($(RGX_FW_SIGNED),1)


endif # RGX_FW_SIGNED

ifeq ($(RGX_FW_SIGNED),1)
$(eval $(call KernelConfigC,RGX_FW_FILENAME,"\"rgx.fw.signed\""))
$(eval $(call KernelConfigC,RGX_SH_FILENAME,"\"rgx.sh.signed\""))
ifneq ($(RGX_FW_X509),)
$(eval $(call KernelConfigC,RGX_FW_SIGNED,1))
endif # RGX_FW_X509
else  # RGX_FW_SIGNED
$(eval $(call KernelConfigC,RGX_FW_FILENAME,"\"rgx.fw\""))
$(eval $(call KernelConfigC,RGX_SH_FILENAME,"\"rgx.sh\""))
endif # RGX_FW_SIGNED

ifneq ($(SUPPORT_ANDROID_PLATFORM),1)
 ifeq ($(wildcard ${TOP}/build/linux/tools/prepare-llvm.sh),)
  # No facility for using LLVM in this package.
 else ifeq ($(LLVM_BUILD_DIR),)
  $(warning LLVM_BUILD_DIR is not set. Components that use it (e.g. OpenCL, Vulkan) cannot be built)
 else
  override LLVM_BUILD_DIR := $(abspath $(LLVM_BUILD_DIR))
  ifeq ($(SUPPORT_NEUTRINO_PLATFORM),1)
    LLVM_MESSAGE=$(shell VARIANT1=${VARIANT1} ${TOP}/build/linux/tools/prepare-llvm.sh -c $(LLVM_BUILD_DIR))
  else
    LLVM_MESSAGE=$(shell ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} ${TOP}/build/linux/tools/prepare-llvm.sh -c $(LLVM_BUILD_DIR))
  endif

  ifneq ($(filter Error:,$(firstword $(LLVM_MESSAGE))),)
   $(info  *** prepare-llvm.sh: $(LLVM_MESSAGE))
   $(error *** LLVM_BUILD_DIR $(LLVM_BUILD_DIR) is not suitable)
  endif

  ifneq ($(filter Warning:,$(firstword $(LLVM_MESSAGE))),)
   $(info  *** prepare-llvm.sh: $(LLVM_MESSAGE))
  endif

  # Because we need to handle MULTIARCH builds, we can't work out the
  # architecture to use in the paths until compile-time.  So leave
  # _LLVM_ARCH_ as a placeholder that will be replaced in the
  # moduledef.
  LLVM_INCLUDE_PATH := $(LLVM_BUILD_DIR)/llvm.src/llvm/include \
                       $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/include

  CLANG_INCLUDE_PATH := $(LLVM_INCLUDE_PATH) \
                        $(LLVM_BUILD_DIR)/llvm.src/clang/include \
                        $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/tools/clang/include

  SPV_INCLUDE_PATH := $(LLVM_INCLUDE_PATH) \
                      $(LLVM_BUILD_DIR)/llvm.src/spv-translator/include \
                      $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/tools/SPV_TRANSLATOR/include

  LLVM_LIB_PATH := $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/lib/

  LLVM_INCLUDE_PATH_HOST := $(LLVM_BUILD_DIR)/llvm.src/llvm/include/ \
                            $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/include/

  CLANG_INCLUDE_PATH_HOST := $(LLVM_INCLUDE_PATH_HOST) \
                             $(LLVM_BUILD_DIR)/llvm.src/clang/include \
                             $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/tools/clang/include

  SPV_INCLUDE_PATH_HOST := $(LLVM_INCLUDE_PATH_HOST) \
                           $(LLVM_BUILD_DIR)/llvm.src/spv-translator/include \
                           $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/tools/SPV_TRANSLATOR/include

  LLVM_LIB_PATH_HOST := $(LLVM_BUILD_DIR)/llvm._LLVM_ARCH_/lib/
 endif
 else
 LLVM_MESSAGE=$(shell ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ${TOP}/build/linux/tools/prepare-llvm-android.sh -a verify -t "$(JNI_CPU_ABI) $(JNI_CPU_ABI_2ND)")

 ifneq ($(filter Warning:,$(firstword $(LLVM_MESSAGE))),)
  $(info  *** prepare-llvm-android.sh: $(LLVM_MESSAGE))
  $(info *** Please update your VNDK with the newer LLVM version. )
 endif
endif






# The SPV library is in the same folder as the LLVM ones as it is built
# as part of LLVM

ifneq ($(SUPPORT_ANDROID_PLATFORM),1)
 ifeq ($(wildcard ${TOP}/build/linux/tools/prepare-nnvm.sh),)
  # No facility for using NNVM in this package.
 else ifeq ($(NNVM_BUILD_DIR),)
  # IMGDNN is not built by default so do nothing if NNVM_BUILD_DIR is not set
 else
  override NNVM_BUILD_DIR := $(abspath $(NNVM_BUILD_DIR))
  NNVM_MESSAGE=$(shell ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} ${TOP}/build/linux/tools/prepare-nnvm.sh -c $(NNVM_BUILD_DIR))

  ifneq ($(filter Error:,$(firstword $(NNVM_MESSAGE))),)
   $(info  *** prepare-nnvm.sh: $(NNVM_MESSAGE))
   $(error *** NNVM_BUILD_DIR $(NNVM_BUILD_DIR) is not suitable)
  endif

  ifneq ($(filter Warning:,$(firstword $(NNVM_MESSAGE))),)
   $(info  *** prepare-nnvm.sh: $(NNVM_MESSAGE))
  endif

  # Because we need to handle MULTIARCH builds, we can't work out the
  # architecture to use in the paths until compile-time.  So leave
  # _NNVM_ARCH_ as a placeholder that will be replaced in the
  # moduledef.
  NNVM_INCLUDE_PATH := $(NNVM_BUILD_DIR)/nnvm.src/include
  NNVM_LIB_PATH := $(NNVM_BUILD_DIR)/nnvm._NNVM_ARCH_/lib/
 endif
endif






$(if $(USE_CCACHE),$(if $(USE_DISTCC),$(error\
Enabling both USE_CCACHE and USE_DISTCC at the same time is not supported)))


# Invariant options for Linux
#
$(eval $(call BothConfigC,PVR_BUILD_DIR,"\"$(PVR_BUILD_DIR)\""))
$(eval $(call BothConfigC,PVR_BUILD_TYPE,"\"$(BUILD)\""))
$(eval $(call BothConfigC,PVRSRV_MODNAME,"\"$(PVRSRV_MODNAME)\""))
$(eval $(call BothConfigMake,PVRSRV_MODNAME,$(PVRSRV_MODNAME)))
$(eval $(call BothConfigC,PVRSYNC_MODNAME,"\"$(PVRSYNC_MODNAME)\""))
$(eval $(call BothConfigMake,PVRSYNC_MODNAME,$(PVRSYNC_MODNAME)))
$(eval $(call BothConfigMake,PVR_BUILD_DIR,$(PVR_BUILD_DIR)))
$(eval $(call BothConfigMake,PVR_BUILD_TYPE,$(BUILD)))

SUPPORT_RGX ?= 1
ifeq ($(SUPPORT_RGX),1)
$(eval $(call BothConfigC,SUPPORT_RGX,1))
$(eval $(call BothConfigMake,SUPPORT_RGX,1))
endif

# Some of the definitions in stdint.h aren't exposed by default in C++ mode,
# unless these macros are defined. To make sure we get these definitions
# regardless of which files include stdint.h, define them here.

# FIXME: We can't use GCC __thread reliably with clang.
ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
else
endif

ifneq ($(DISPLAY_CONTROLLER),)
$(eval $(call BothConfigC,DISPLAY_CONTROLLER,$(DISPLAY_CONTROLLER)))
$(eval $(call BothConfigMake,DISPLAY_CONTROLLER,$(DISPLAY_CONTROLLER)))
endif

ifneq ($(DMA_CONTROLLER),)
endif


$(eval $(call BothConfigMake,PVR_SYSTEM,$(PVR_SYSTEM)))
$(eval $(call KernelConfigMake,PVR_LOADER,$(PVR_LOADER)))

ifeq ($(MESA_EGL),1)
else ifeq ($(SC_EGL),1)
else
endif

# Build-type dependent options
#
$(eval $(call BothConfigMake,BUILD,$(BUILD)))

ifeq ($(SUPPORT_VALIDATION),1)
SUPPORT_TRP :=\
 $(shell grep -qw RGX_FEATURE_TILE_REGION_PROTECTION $(RGX_BNC_CONFIG_KM) && echo 1)

# Enable Periodic Hardware Reset functionality for testing
override PVR_ENABLE_PHR := 1
# Enable CSW register decode
override PVRSRV_DEBUG_CSW_STATE := 1
endif

ifeq ($(SUPPORT_PERFORMANCE_RUN),1)
PVRSRV_APPHINT_HWPERFDISABLECOUNTERFILTER := 1
endif


# Sort bridge structure members according to size from biggest to smallest
# members.
#
$(eval $(call TunableBothConfigMake,SORT_BRIDGE_STRUCTS,1))

# Prevent excluding regconfig bridge when the build level macro defined,
# regconfig functions are used in pvrdebug.
#
ifeq ($(SUPPORT_USER_REGISTER_CONFIGURATION),1)
ifeq ($(EXCLUDE_RGXREGCONFIG_BRIDGE),1)
override EXCLUDE_RGXREGCONFIG_BRIDGE := 0
endif
endif

ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
# Always use DEBUGLINK. These days we are using external components which
# have large amounts of C++ debug information and it is impractical to carry
# statically linked components to the target filesystem without stripping.
DEBUGLINK ?= 1
endif

ifeq ($(BUILD),debug)

$(eval $(call TunableBothConfigMake,PVR_SERVICES_DEBUG,,\
Enable additional services debug options._\
This needs to be enabled for both the UM and KM builds_\
so that compatibility between them is achieved.\
))

ifeq ($(PVR_SERVICES_DEBUG),1)
PVRSRV_ENABLE_GPU_MEMORY_INFO ?= 1
PVRSRV_DEBUG_HANDLE_LOCK ?= 1
PVRSRV_APPHINT_ENABLEFWPOISONONFREE ?= IMG_TRUE
PVRSRV_TIMER_CORRELATION_HISTORY ?= 1
endif

# enable sync prim poisoning in debug builds
PVRSRV_ENABLE_SYNC_POISONING ?= 1

# bridge debug and statistics enabled by default in debug builds
DEBUG_BRIDGE_KM ?= 1
$(eval $(call BothConfigC,DEBUG,))
$(eval $(call TunableBothConfigMake,DEBUGLINK,))
PERFDATA ?= 1
TRACK_FW_BOOT ?= 1

else ifeq ($(BUILD),release)
$(eval $(call BothConfigC,RELEASE,))
$(eval $(call TunableBothConfigMake,DEBUGLINK,1))


else ifeq ($(BUILD),timing)
$(eval $(call BothConfigC,TIMING,))
$(eval $(call TunableBothConfigMake,DEBUGLINK,1))
else
$(error BUILD= must be either debug, release or timing)

endif # $BUILD


# Memtest, currently implemented for Linux only
$(eval $(call TunableKernelConfigC,SUPPORT_PHYSMEM_TEST,1))
$(eval $(call TunableKernelConfigMake,SUPPORT_PHYSMEM_TEST,1))


$(eval $(call TunableKernelConfigMake,KERNEL_DEBUGLINK,,\
Enable this to store DDK kernel module debugging symbols in separate$(comma) per_\
module$(comma) .dbg files. These files will not be installed on the target system$(comma)_\
but can be used by tools$(comma) e.g. gdb$(comma) for offline debugging. This may be_\
desirable when the target system has limited storage space and the kernel_\
has been configured with CONFIG_DEBUG_INFO=y$(comma) which can have a significant_\
impact on kernel module size.))

$(eval $(call TunableBothConfigMake,COMPRESS_DEBUG_SECTIONS,,\
Enable compression on debug sections (.zdebug)_\
May have tool compatibility issues.))


ifneq ($(SUPPORT_ALT_REGBASE),)
$(eval $(call KernelConfigC,SUPPORT_ALT_REGBASE,,\
Some systems alter the GPU addresses before they reach the bus, e.g. by appending_\
a MSB bit. In such cases, the GPU regbank in the system address space might alias_\
from the GPU perspective with memory addresses used by the GPU. For example, if_\
the GPU regbank is located in the lower 1GB and the 32bit GPU accesses the range_\
1GB-5GB due to the address alteration. In such cases, the MIPS wrapper must use_\
an alternate register bank address to avoid aliasing with device memory. The address_\
used must be an address that is not mapped in the GPU as memory. The memory backing_\
those addresses is never touched since the access is resolved inside the GPU.\
\))
endif

# User-configurable options
#
ifneq ($(RGX_BVNC),)
$(eval $(call TunableKernelConfigC,RGX_BVNC_CORE_KM_HEADER,))
endif
ifneq ($(RGX_BVNC),)
$(eval $(call TunableKernelConfigC,RGX_BNC_CONFIG_KM_HEADER,))
endif

$(eval $(call TunableBothConfigC,PVRSRV_DEBUG_HANDLE_LOCK,,\
Enable checking that the handle lock is held when a handle reference_\
count is modified))

$(eval $(call TunableBothConfigC,PVR_DBG_BREAK_ASSERT_FAIL,,\
Enable this to treat PVR_DBG_BREAK as PVR_ASSERT(0)._\
Otherwise it is ignored._\
))
$(eval $(call TunableBothConfigC,PDUMP,,\
Enable parameter dumping in the driver._\
This adds code to record the parameters being sent to the hardware for_\
later analysis._\
))
$(eval $(call TunableBothConfigC,NO_HARDWARE,,\
Disable hardware interactions (e.g. register writes) that the driver would_\
normally perform. A driver built with this option can$(apos)t drive hardware$(comma)_\
but with PDUMP enabled$(comma) it can capture parameters to be played back later._\
))
$(eval $(call TunableBothConfigC,PDUMP_DEBUG_OUTFILES,,\
Add debug information to the pdump script (out2.txt) as it is generated._\
This includes line numbers$(comma) process names and also enables checksumming_\
of the binary data dumped to out2.prm which can be verified offline._\
))
$(eval $(call TunableKernelConfigC,PVRSRV_NEED_PVR_DPF,1,\
Enables PVR_DPF messages in the kernel mode driver._\
))
$(eval $(call TunableBothConfigC,PVRSRV_NEED_PVR_ASSERT,,\
Enable this to turn on PVR_ASSERT in release builds._\
))
$(eval $(call TunableBothConfigC,PVRSRV_NEED_PVR_TRACE,,\
Enable this to turn on PVR_TRACE in release builds._\
))
$(eval $(call TunableBothConfigC,PVRSRV_NEED_PVR_STACKTRACE,,\
Enable this to turn on stack trace functionality requiring either native_\
operating system features$(comma) or additional dependencies such as_\
libunwind._\
))
ifeq ($(PVRSRV_NEED_PVR_STACKTRACE),1)
ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
$(warning Since Android O it's not allowed to link to libunwind.)
endif
endif
$(eval $(call TunableBothConfigC,REFCOUNT_DEBUG,))
$(eval $(call TunableBothConfigC,DC_DEBUG,,\
Enable debug tracing in the DC (display class) server code))
$(eval $(call TunableBothConfigC,SCP_DEBUG,,\
Enable debug tracing in the SCP (software command processor)_\
which is used by the DC.))
$(eval $(call TunableBothConfigC,SUPPORT_INSECURE_EXPORT,))
$(eval $(call TunableBothConfigC,SUPPORT_SECURE_EXPORT,,\
Enable support for secure device memory and sync export._\
This replaces export handles with file descriptors$(comma) which can be passed_\
between processes to share memory._\
))
$(eval $(call TunableBothConfigC,SUPPORT_DISPLAY_CLASS,,\
Enable DC (display class) support. Disable if not using a DC display driver.))
$(eval $(call TunableBothConfigC,PVRSRV_DEBUG_CCB_MAX,))

$(eval $(call TunableBothConfigMake,SUPPORT_TRUSTED_DEVICE,))
$(eval $(call TunableBothConfigC,SUPPORT_TRUSTED_DEVICE,,\
Enable a build mode targeting an REE._\
))

ifeq ($(SUPPORT_TRUSTED_DEVICE),1)
ifeq ($(NO_HARDWARE),1)
SUPPORT_SECURITY_VALIDATION ?= 1
endif
endif

$(eval $(call TunableBothConfigC,SUPPORT_SECURITY_VALIDATION,,\
Enable DRM security validation mode._\
))
$(eval $(call TunableBothConfigMake,SUPPORT_SECURITY_VALIDATION,))

$(eval $(call TunableBothConfigC,PM_INTERACTIVE_MODE,,\
Enable PM interactive mode._\
))

$(eval $(call TunableBothConfigC,SECURE_FW_CODE_OSID,,\
Emit specified OSID when the FW fetches code from memory._\
In MIPS this will only work for statically mapped FW code._\
))

ifeq ($(SUPPORT_TRUSTED_DEVICE),1)
override SUPPORT_MIPS_CONTIGUOUS_FW_MEMORY := 1
endif


$(eval $(call TunableBothConfigC,SUPPORT_RGXTQ_BRIDGE,1,\
Enable RGXTQ bridge which is always present on Rogue cores_\
))



$(eval $(call TunableBothConfigC,SUPPORT_MIPS_CONTIGUOUS_FW_MEMORY,,\
Use a single big allocation for the FW code and another one_\
for the FW private data._\
))

$(eval $(call TunableBothConfigC,SUPPORT_MIPS_UNIFIED_FW_MEMORY,,\
Map both MIPS FW code and data in a single MIPS TLB entry. Code and data_\
need to be adjacent in both virtual and physical memory space._\
))

$(eval $(call TunableBothConfigC,METRICS_USE_ARM_COUNTERS,,\
Enable usage of hardware performance counters for metrics on ARM platforms._\
))


ifeq ($(SHADER_DEBUG_TOOL),1)
 ifneq ($(GTRACE_TOOL),1)
  override GTRACE_TOOL = 1
  $(warning SHADER_DEBUG_TOOL requires GTRACE_TOOL, so GTRACE_TOOL is being enabled.)
 endif
endif
ifeq ($(GTRACE_TOOL),1)
 ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
 endif
 override PVR_ANNOTATION_MAX_LEN ?= 128
endif


IMG_1_11_OPTS?=0xffffffff

IMG_1_12_OPTS?=0xffffffff





# poison values for the Services
$(eval $(call TunableBothConfigC,PVRSRV_POISON_ON_ALLOC_VALUE,0xd9,\
Poison on alloc value))
$(eval $(call TunableBothConfigC,PVRSRV_POISON_ON_FREE_VALUE,0x63,\
Poison on free value))

$(eval $(call TunableBothConfigC,SUPPORT_MIPS_64K_PAGE_SIZE,,\
Enable this to change the MIPS FW page size to 64K._\
))

#
# GPU virtualization support
#
VMM_TYPE ?= stub
RGX_FW_HEAP_SHIFT ?= 25

ifeq ($(PVRSRV_VZ_NUM_OSID),0)
 override PVRSRV_VZ_NUM_OSID := 1
endif

# Make RGX_NUM_OS_SUPPORTED visible to both UM & KM makefiles
$(eval $(call BothConfigMake,RGX_NUM_OS_SUPPORTED,$(PVRSRV_VZ_NUM_OSID),))
# Reveal RGX_NUM_OS_SUPPORTED only to KM code, allowing the firmware makefiles,
# which are part of the UM, to selectively control this symbol so the same DDK
# build can create both native and vz-supported firmware binaries
$(eval $(call KernelConfigC,RGX_NUM_OS_SUPPORTED,$(PVRSRV_VZ_NUM_OSID),\
The number of firmware supported OSIDs [1 native build : 2+ vz build]))

# Default values for virtualisation QoS parameters
ifeq ($(RGX_OSID_0_DEFAULT_PRIORITY),)
 override RGX_OSID_0_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 0)
endif
ifeq ($(RGX_OSID_1_DEFAULT_PRIORITY),)
 override RGX_OSID_1_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 1)
endif
ifeq ($(RGX_OSID_2_DEFAULT_PRIORITY),)
 override RGX_OSID_2_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 2)
endif
ifeq ($(RGX_OSID_3_DEFAULT_PRIORITY),)
 override RGX_OSID_3_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 3)
endif
ifeq ($(RGX_OSID_4_DEFAULT_PRIORITY),)
 override RGX_OSID_4_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 4)
endif
ifeq ($(RGX_OSID_5_DEFAULT_PRIORITY),)
 override RGX_OSID_5_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 5)
endif
ifeq ($(RGX_OSID_6_DEFAULT_PRIORITY),)
 override RGX_OSID_6_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 6)
endif
ifeq ($(RGX_OSID_7_DEFAULT_PRIORITY),)
 override RGX_OSID_7_DEFAULT_PRIORITY := ($(PVRSRV_VZ_NUM_OSID) - 7)
endif
ifeq ($(RGX_HCS_DEFAULT_DEADLINE_MS),)
 override RGX_HCS_DEFAULT_DEADLINE_MS := 0xFFFFFFFFU
endif

$(eval $(call KernelConfigC,RGX_OSID_0_DEFAULT_PRIORITY,$(RGX_OSID_0_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_OSID_1_DEFAULT_PRIORITY,$(RGX_OSID_1_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_OSID_2_DEFAULT_PRIORITY,$(RGX_OSID_2_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_OSID_3_DEFAULT_PRIORITY,$(RGX_OSID_3_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_OSID_4_DEFAULT_PRIORITY,$(RGX_OSID_4_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_OSID_5_DEFAULT_PRIORITY,$(RGX_OSID_5_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_OSID_6_DEFAULT_PRIORITY,$(RGX_OSID_6_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_OSID_7_DEFAULT_PRIORITY,$(RGX_OSID_7_DEFAULT_PRIORITY),))
$(eval $(call KernelConfigC,RGX_HCS_DEFAULT_DEADLINE_MS,$(RGX_HCS_DEFAULT_DEADLINE_MS),))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_DRIVERMODE,0x7FFFFFFF,\
The driver execution mode. This can be either an override or non-override 32-bit value. An override_\
value has the MSB bit set and a non-override value has this bit cleared. Excluding this MSB bit_\
and treating the remaining 31-bit value as a signed integer the mode values are_\
[-1 native mode <default>: 0 host mode : +1 guest mode]))
$(eval $(call KernelConfigMake,VMM_TYPE,$(VMM_TYPE),\
The virtual machine manager type, defaults to stub implementation))
$(eval $(call TunableBothConfigC,RGX_FW_HEAP_SHIFT,$(RGX_FW_HEAP_SHIFT),\
Firmware physical heap log2 size per OSID (minimum 4MiB, default 32MiB).))

ifeq ($(SUPPORT_FIRMWARE_UNITTESTS),1)
# These configs are planned to be off on SC builds, hence the unittests don't test them
override SUPPORT_POWMON_COMPONENT := 0
override PVR_GPIO_MODE := 0
endif

$(eval $(call TunableBothConfigC,SUPPORT_AUTOVZ,,\
Enable support for AutoVz mode_))

ifeq ($(SUPPORT_AUTOVZ),1)
 override RGX_VZ_STATIC_CARVEOUT_FW_HEAPS := 1

 $(eval $(call TunableBothConfigC,SUPPORT_AUTOVZ_HW_REGS,1,\
 Use dedicated hardware registers for tracking OS states otherwise rely on shared memory._))

 $(eval $(call TunableBothConfigC,PVR_AUTOVZ_WDG_PERIOD_MS,3000,\
 Time period in milliseconds between the firmware autovz watchdog checks._))
endif

$(eval $(call TunableBothConfigC,PVR_AUTOVZ_OVERRIDE_FW_MMU_CARVEOUT_BASE_ADDR,,\
The carveout memory used for allocating the firmware MMU pagetables it start by default after_\
the firmware heap. If a different memory range is reserved for this purpose set its base address._))

$(eval $(call TunableBothConfigC,RGX_VZ_STATIC_CARVEOUT_FW_HEAPS,,\
Firmware heaps of Guest VMs are allocated from pre-determined carveout memory.))

$(eval $(call TunableBothConfigMake,PVR_ENABLE_PHR,,))
$(eval $(call TunableBothConfigC,PVR_ENABLE_PHR,,\
Enable the Periodic Hardware Reset functionality (PHR)._))

$(eval $(call TunableBothConfigC,RGX_FW_IRQ_OS_COUNTERS,))
$(eval $(call TunableBothConfigC,RGX_IRQ_HYPERV_HANDLER,))

$(eval $(call TunableBothConfigC,FIX_DUSTS_POW_ON_INIT,,\
Enable WA for power controllers that power up dusts by default._\
The Firmware powers down the dusts after booting._\
))
$(eval $(call TunableKernelConfigMake,SUPPORT_LINUX_DVFS,))
$(eval $(call TunableKernelConfigC,SUPPORT_LINUX_DVFS,,\
Enables PVR DVFS implementation to actively change frequency / voltage depending_\
on current GPU load. Currently only supported on Linux._\
))

#
# GPU power monitoring configuration
#

$(eval $(call TunableBothConfigMake,SUPPORT_POWMON_COMPONENT,1))
$(eval $(call TunableBothConfigC,SUPPORT_POWMON_COMPONENT,1,\
Includes power-monitoring component in firmware build._\
))

ifeq ($(SUPPORT_POWMON_COMPONENT),0)
# Force disable power-sampling if powmon component not being compiled in
 override SUPPORT_POWER_SAMPLING_VIA_DEBUGFS := 0
endif

ifneq ($(SUPPORT_POWMON_COMPONENT),0)
# Following tunables are only used when power-monitoring present
$(eval $(call TunableBothConfigC,PVR_POWER_ACTOR,,\
Enables PVR power actor implementation for registration with a kernel configured_\
with IPA. Enables power counter measurement timer in the FW which is periodically_\
read by the host DVFS from the POWER_ESTIMATE register in order to operate within_\
a governor set power envelope.))
$(eval $(call TunableBothConfigC,PVR_POWER_ACTOR_MEASUREMENT_PERIOD_MS,10U,\
Period of time between regular power measurements. Default 10ms))
$(eval $(call BothConfigC,PVR_POWER_MONITOR_HWPERF,,\
Enables the generation of hwperf power monitoring packets._\
This incurs an additional performance cost.))
$(eval $(call TunableBothConfigC,PVR_POWER_MONITOR_DYNAMIC_ENERGY,,\
Configures the power monitoring module to calculate dynamic energy_\
instead of the default total power.))
$(eval $(call TunableBothConfigC,PVR_POWER_ACTOR_SCALING,,\
Scaling factor for the dynamic power coefficients.))
$(eval $(call TunableBothConfigC,SUPPORT_POWER_SAMPLING_VIA_DEBUGFS,,\
Enable sampling of power counter registers and expose the values via DebugFS power_data file.))
 ifneq ($(SUPPORT_POWER_VALIDATION_VIA_DEBUGFS),)
 $(error SUPPORT_POWER_VALIDATION_VIA_DEBUGFS is currently unsupported on Rogue.)
 endif
endif


ifneq ($(PVR_POWER_ACTOR),)


else


endif

$(eval $(call TunableKernelConfigC,DEBUG_HANDLEALLOC_INFO_KM,))
$(eval $(call TunableKernelConfigC,PVRSRV_RESET_ON_HWTIMEOUT,))
$(eval $(call TunableKernelConfigC,PVR_LDM_PLATFORM_PRE_REGISTERED,))
$(eval $(call TunableKernelConfigC,PVR_LDM_DRIVER_REGISTRATION_NAME,"\"$(PVRSRV_MODNAME)\""))
$(eval $(call TunableBothConfigC,PVRSRV_ENABLE_SYNC_POISONING,,\
Poison Sync Prim values on free.))
$(eval $(call TunableBothConfigC,PVRSRV_FULL_SYNC_TRACKING_HISTORY_LEN,256))
$(eval $(call TunableKernelConfigC,SYNC_DEBUG,))
$(eval $(call TunableKernelConfigC,SUPPORT_DUMP_CLIENT_CCB_COMMANDS,))
$(eval $(call TunableKernelConfigC,SUPPORT_MMU_PAGESIZECONFIG_REFCOUNT,))

# Disable DDK features when PDUMP is defined
#
ifeq ($(PDUMP),1)
# Increase the default max annotation length when PDump is enabled
override PVR_ANNOTATION_MAX_LEN ?= 96
endif

$(eval $(call TunableKernelConfigC,SUPPORT_DC_COMPLETE_TIMEOUT_DEBUG,))

$(eval $(call TunableBothConfigC,SUPPORT_PVR_VALGRIND,))

$(eval $(call TunableBothConfigC,ION_DEFAULT_HEAP_NAME,\"system\",\
In ion enabled DDKs$(comma) this setting should be the default heap name defined in the kernel_\
where gralloc buffers are allocated by default._\
This setting is for kernels (>=4.12) which support ION heap query_\
))
$(eval $(call TunableBothConfigC,ION_DEFAULT_HEAP_ID_MASK,(1 << ION_HEAP_TYPE_SYSTEM),\
In ion enabled DDKs$(comma) this setting should be (1 << ION_HEAP_TYPE_xxx)_\
where ION_HEAP_TYPE_xxx is default heap id defined in the kernel._\
This setting is for kernels (<4.12) which do not support ION heap query_\
))
$(eval $(call TunableBothConfigC,ION_FALLBACK_HEAP_NAME,,\
In ion enabled DDKs$(comma) use this setting to define fallback heap._\
This setting is optional (depends if kernel has defined a fallback heap)_\
))
$(eval $(call TunableBothConfigC,ION_FALLBACK_HEAP_ID_MASK,,\
In ion enabled DDKs$(comma) use this setting to define fallback heap id mask._\
This setting is optional (depends if kernel has defined a fallback heap)_\
))

EGL_EXTENSION_KHR_GL_COLORSPACE ?= 1


ifneq ($(MESA_EGL),1)
 # The EGL EXT_image_gl_colorspace spec says:
 #    "Can be supported on EGL 1.4 provided that EGL_KHR_gl_colorspace is
 #     implemented, as well as either EGL_KHR_image or EGL_KHR_image_base."
 #
 # Note: IMG EGL supports EGL_KHR_image and EGL_KHR_image_base unconditionally.
 ifeq ($(EGL_EXTENSION_KHR_GL_COLORSPACE),1)
 endif


 # Check for GBM_BACKENDs that aren't understood by the GBM Linux.mk.
 $(eval $(call ValidateValues,GBM_BACKEND,$(_supported_gbm_backends)))

endif


$(eval $(call TunableBothConfigC,PVRSRV_DEVMEM_TEST_SAFE_MEMSETCPY,,\
Enable this to force the use of PVRSRVMemSet/Copy in the client driver _\
instead of the built-in libc functions. These implementations are device _\
memory safe and are used by default on AARCH64 platform._\
))

$(eval $(call TunableBothConfigC,PVRSRV_BRIDGE_LOGGING,,\
If enabled$(comma) provides a debugfs entry which logs the number of calls_\
made to each bridge function._\
))

$(eval $(call TunableKernelConfigC,PVRSRV_SERVER_THREADS_INDEFINITE_SLEEP,,\
If enabled it will make kernel threads to sleep indefinitely until signalled._\
))

# If we are building against a ChromeOS kernel, set this.
$(eval $(call TunableKernelConfigC,CHROMIUMOS_KERNEL,))
$(eval $(call TunableKernelConfigMake,CHROMIUMOS_KERNEL,))




ifneq ($(SUPPORT_ANDROID_PLATFORM),1)
endif


DEFERRED_WORKER_THREAD ?= 1


$(eval $(call TunableKernelConfigC,CACHEFLUSH_NO_KMRBF_USING_UMVA,))
$(eval $(call TunableBothConfigC,CACHEFLUSH_ISA_TYPE,,\
Specify CPU d-cache maintenance ISA type (i.e. CACHEFLUSH_ISA_TYPE_[X86,ARM64,GENERIC])._\
))

# Overrides the size of the Vulkan host-visible heap with the supplied size in MB



ifneq ($(BUILD),release)
ZEUS_SUPPORT_RENDER_INFO ?= 1
endif






# Support version 3 of the loader. Versions 0/1/2 should not be used.
# Default needs to be set separately otherwise 0 gets replaced with the default
SUPPORT_LUNARG_LOADER_VERSION ?= 3

ifeq ($(SUPPORT_VKEXT_IMAGE_FORMAT_MOD),1)
endif

ifneq ($(SUPPORT_ANDROID_PLATFORM),1)
ifeq ($(WINDOW_SYSTEM),lws-generic)
# Check for SUPPORT_VK_PLATFORMS that aren't understood by the Vulkan Linux.mk.
# Platform names match Mesa where possible.
_supported_vk_platforms := null tizen wayland x11
$(eval $(call ValidateValues,SUPPORT_VK_PLATFORMS,$(_supported_vk_platforms)))

else
endif

ifneq ($(SUPPORT_VK_PLATFORMS),)
endif
endif

ifeq ($(PVR_BLOB_CACHE_DEBUG),1)
$(eval $(call BothConfigC,BLOB_CACHE_DEBUG,))
endif


$(eval $(call TunableBothConfigMake,PDUMP,))
$(eval $(call TunableBothConfigMake,SUPPORT_INSECURE_EXPORT,))
$(eval $(call TunableBothConfigMake,SUPPORT_SECURE_EXPORT,))
$(eval $(call TunableBothConfigMake,SUPPORT_DISPLAY_CLASS,))
$(eval $(call TunableBothConfigMake,CLDNN,,\
Build CLDNN graph libraries._\
))
$(eval $(call TunableBothConfigC,SUPPORT_EXTRA_METASP_DEBUG,,\
Enable extra debug information using the META Slave Port._\
Checks the validity of the Firmware code and dumps sync values_\
using the GPU memory subsystem via the META Slave Port._\
))
$(eval $(call TunableBothConfigC,TRACK_FW_BOOT,,Enable FW boot tracking.))
# Required to pass the build flag to the META FW makefile

$(eval $(call TunableBothConfigMake,OPTIM,,\
Specify the optimisation flags passed to the compiler. Normally this_\
is autoconfigured based on the build type._\
))










# Enables the pre-compiled header use.

# Switch among glsl frontend compiler variants

RGX_TIMECORR_CLOCK ?= mono
$(eval $(call TunableKernelConfigMake,RGX_TIMECORR_CLOCK,mono,\
Specifies which clock source will be used for time correlation tables and_\
HWPerf))
ifeq ($(RGX_TIMECORR_CLOCK),mono)
PVRSRV_APPHINT_TIMECORRCLOCK=0
else ifeq ($(RGX_TIMECORR_CLOCK),mono_raw)
PVRSRV_APPHINT_TIMECORRCLOCK=1
else ifeq ($(RGX_TIMECORR_CLOCK),sched)
PVRSRV_APPHINT_TIMECORRCLOCK=2
else
$(error Wrong value ($(RGX_TIMECORR_CLOCK)) for RGX_TIMECORR_CLOCK)
endif

# HWR is enabled by default
ASSERTONHWRTRIGGER_DEFAULT_ENABLED ?= 0
$(eval $(call TunableBothConfigC,ASSERTONHWRTRIGGER_DEFAULT_ENABLED,))

# Build-only AppHint configuration values
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWRDEBUGDUMPLIMIT,APPHNT_BLDVAR_DBGDUMPLIMIT,\
Limit for the number of HWR debug dumps produced))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLETRUSTEDDEVICEACECONFIG,IMG_FALSE,\
Enable trusted device ACE config))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_GENERALNON4KHEAPPAGESIZE,0x4000,\
Alternate general heap page size (i.e. 4K,16K [default],64K,256K,1M,2M)))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFCLIENTBUFFERSIZE,786432,\
Buffer size in bytes for client HWPerf streams))

# PDUMP AppHint defaults
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLESIGNATURECHECKS,APPHNT_BLDVAR_ENABLESIGNATURECHECKS,\
Buffer size in bytes for storing signature check data))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_SIGNATURECHECKSBUFSIZE,RGXFW_SIG_BUFFER_SIZE_MIN,\
Buffer size in bytes for storing signature check data))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEFULLSYNCTRACKING,IMG_FALSE,\
Enable full sync tracking))
ifeq ($(MTK_MINI_PORTING),1)
$(shell echo "#if defined(CONFIG_MTK_ENABLE_GMO)" >> $(CONFIG_KERNEL_H).new)
endif
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEPAGEFAULTDEBUG,APPHNT_BLDVAR_ENABLEPAGEFAULTDEBUG,\
Enable page fault debug))
ifeq ($(MTK_MINI_PORTING),1)
$(shell echo "#else" >> $(CONFIG_KERNEL_H).new)
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEPAGEFAULTDEBUG,IMG_TRUE,\
Enable page fault debug))
$(shell echo "#endif" >> $(CONFIG_KERNEL_H).new)
endif

# Validation AppHint defaults
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_VALIDATEIRQ,0,\
Used to validate the interrupt integration. \
Enables extra code in the FW to assert all interrupt lines \
at boot and polls on the host side. The code is only built when \
generating pdumps for nohw targets.))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_DISABLECLOCKGATING,0,\
Disable GPU clock gating))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_DISABLEDMOVERLAP,0,\
Disable GPU data master overlapping))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLECDMKILLINGRANDMODE,0,\
Enable random killing of the compute data master))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLERANDOMCONTEXTSWITCH,0,\
Enable random context switching of all DMs for debug and testing purposes))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLESOFTRESETCNTEXTSWITCH,0,\
Enable soft GPU resets on context switching))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEFWCONTEXTSWITCH,RGXFWIF_INICFG_OS_CTXSWITCH_DM_ALL,\
Enable firmware context switching))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_VDMCONTEXTSWITCHMODE,RGXFWIF_INICFG_VDM_CTX_STORE_MODE_INDEX,\
Enable VDM context switching mode))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLERDPOWERISLAND,RGX_RD_POWER_ISLAND_DEFAULT,\
Enable RD power island))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_FIRMWAREPERF,FW_PERF_CONF_NONE,\
Force the initial Firmware Performance Configuration to the specified value))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_FWCONTEXTSWITCHPROFILE,RGXFWIF_CTXSWITCH_PROFILE_MEDIUM_EN,\
Firmware context switch profile))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFDISABLECUSTOMCOUNTERFILTER,0,\
Force the initial HW Performance Custom Counter Filter value))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFFWBUFSIZEINKB,2048,\
Buffer size in KB of the hardware performance GPU buffer))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFHOSTBUFSIZEINKB,2048,\
Buffer size in KB of the hardware performance host buffer))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFHOSTTHREADTIMEOUTINMS,50,\
Timeout in milliseconds of the hardware performance host thread))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_JONESDISABLEMASK,0,\
Disable Jones))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_NEWFILTERINGMODE,1,\
Enable new TPU filtering mode))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_TRUNCATEMODE,0,\
Truncate mode))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_EMUMAXFREQ,0,\
Set the maximum frequency for the emulator))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_GPIOVALIDATIONMODE,0,\
Set the validation type for GPIO 1 for the standard protocol validation\
2 for the address protocol validation))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_RGXBVNC,\"\",\
Array of comma separated strings that define BVNC info of the devices.\
(module parameter usage is RGXBVNC=x.x.x.x,y.y.y.y etc)))

# Build-only AppHint configuration values
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_CLEANUPTHREADPRIORITY,5,\
Set the priority of the cleanup thread (0 - default, 1 - highest, 5 - lowest)))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_WATCHDOGTHREADPRIORITY,0,\
Set the priority of the watchdog thread (0 - default, 1 - highest, 5 - lowest)))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_CACHEOPTHREADPRIORITY,1,\
Set the priority of the pvrcache_ops thread (0 - default, 1 - highest, 5 - lowest)))

# Debugfs AppHint configuration values
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ASSERTONHWRTRIGGER,IMG_FALSE,\
Enable firmware assert when an HWR event is triggered))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ASSERTOUTOFMEMORY,IMG_FALSE,\
Enable firmware assert when the TA raises out-of-memory))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_CHECKMLIST,APPHNT_BLDVAR_DEBUG,\
Enable firmware MLIST consistency checker))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_DISABLEFEDLOGGING,IMG_FALSE,\
Disable fatal error detection debug dumps))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_KCCB_SIZE_LOG2,7,\
Adjust KCCB size((7 - default, 16 - highest, 4 - lowest))))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEAPM,RGX_ACTIVEPM_DEFAULT,\
Force the initial driver APM configuration to the specified value))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEHTBLOGGROUP,0,\
Enable host trace log groups))
$(eval $(call AppHintFlagsConfigC,PVRSRV_APPHINT_ENABLELOGGROUP,RGXFWIF_LOG_TYPE_NONE,\
RGXFWIF_LOG_TYPE_GROUP_,BIF CLEANUP CSW DEBUG DMA HWP HWR MAIN MTS PM POW RPM RTD SPM,\
Enable firmware trace log groups))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_FIRMWARELOGTYPE,0,\
Specify output mechanism for firmware log data. 0 = TRACE and 1 = TBI))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_FWTRACEBUFSIZEINDWORDS,$\
RGXFW_TRACE_BUF_DEFAULT_SIZE_IN_DWORDS,Buffer size in DWords for FW trace log data))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_FBCDCVERSIONOVERRIDE,0,\
Override system layer FBCDC version settings \
(0) No override \
(1) Force v3 \
(2) Force v3.1))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HTBOPERATIONMODE,HTB_OPMODE_DROPOLDEST,\
Configure host trace buffer behaviour))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HTBUFFERSIZE,64,\
Buffer size in Kbytes for Host Trace log data))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEFTRACEGPU,IMG_FALSE,\
Enables generation of GPU FTrace events))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFFWFILTER,0,\
Mask used to select GPU events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFHOSTFILTER,0,\
Mask used to select host events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFCLIENTFILTER_SERVICES,0,\
Mask used to select client Services events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFCLIENTFILTER_EGL,0,\
Mask used to select client EGL events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFCLIENTFILTER_OPENGLES,0,\
Mask used to select client OpenGLES events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFCLIENTFILTER_OPENCL,0,\
Mask used to select client OpenCL events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFCLIENTFILTER_VULKAN,0,\
Mask used to select client Vulkan events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_HWPERFCLIENTFILTER_OPENGL,0,\
Mask used to select client OpenGL events to log for performance))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_TIMECORRCLOCK,0,\
Select HWPerf clock))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ENABLEFWPOISONONFREE,IMG_FALSE,\
Enables poisoning of firmware allocations when freed))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_FWPOISONONFREEVALUE,0xBD,\
Poison value when PVRSRV_APPHINT_ENABLEFWPOISONONFREE is enabled))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_ZEROFREELIST,IMG_FALSE,\
Zero freelist memory during freelist reconstruction as part of HWR))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_GPUUNITSPOWERCHANGE,IMG_FALSE,\
Setting this to '1' enables a test mode to dynamically change the_\
DUSTs powered before each kick to the FW))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_DISABLEPDUMPPANIC,IMG_FALSE,\
Disable PDUMP panic))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_CACHEOPCONFIG,0,\
CPU d-cache maintenance framework <CacheOp> flush type configuration))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_CACHEOPUMKMHRESHOLDSIZE,0,\
CPU d-cache maintenance framework <CacheOp> UM/KM threshold configuration))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_IGNOREHWREPORTEDBVNC,IMG_FALSE,\
Ignore BVNC reported by HW and use the value specified at build time))
$(eval $(call AppHintConfigC,PVRSRV_APPHINT_PHYSMEMTESTPASSES,$\
APPHNT_PHYSMEMTEST_ENABLE,Set number of passes of driver start time MemTest))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_TESTSLRINTERVAL,0,\
Interval which determines after how many 3D kicks the driver will_\
insert an additional unsignalled sync checkpoint to the 3D fence_\
allowing testing of SLR))

$(eval $(call AppHintConfigC,PVRSRV_APPHINT_RISCVDMITEST,0,\
Enable RISC-V FW DMI test))

# GLSL compiler options
ifeq ($(BUILD),debug)
DUMP_LOGFILES ?= 1
endif
# end of GLSL compiler options


$(eval $(call TunableBothConfigC,SUPPORT_AXI_ACE_TEST,,\
Enable this to add extra FW code for the AXI ACE unittest._\
))


TQ_CAPTURE_PARAMS ?= 1

TQ_DISABLE_SPARSE ?= 0

$(eval $(call TunableBothConfigC,RGXFW_DEBUG_LOG_GROUP,,\
Enable the usage of DEBUG log group in the Firmware logs._\
))

$(eval $(call TunableBothConfigC,SUPPORT_SOC_TIMER,,\
Enable the use of the SoC timer. When enabled the SoC system layer must implement the pfnSoCTimerRead_\
callback that when invoked returns the current value of the SoC timer._\
))
$(eval $(call TunableBothConfigC,SOC_TIMER_FREQ,20,\
The SoC timer frequency. This default to 20 MHz if not specified_\
))

#
# Tile Lifetime tracking support.
#
ifeq ($(SUPPORT_COMPUTE_ONLY),)
ifeq ($(SUPPORT_VALIDATION),1)
SUPPORT_TLT_PERF ?= 1
endif
endif


#
# Strip Rendering support.
#
$(eval $(call TunableBothConfigMake,SUPPORT_STRIP_RENDERING,,\
Enabling this feature provides the ability for the firmware to drive the_\
display controller via GPIO and support Strip Rendering._\
))
$(eval $(call TunableBothConfigC,SUPPORT_STRIP_RENDERING,))

$(eval $(call TunableBothConfigMake,SUPPORT_DEDICATED_FW_MEMORY,,\
Allocate FW code and private data from dedicated FW memory._\
))
$(eval $(call TunableBothConfigC,SUPPORT_DEDICATED_FW_MEMORY,))



#
# Ensure top-level PDVFS build defines are set correctly
#
ifeq ($(SUPPORT_PDVFS),1)
SUPPORT_WORKLOAD_ESTIMATION ?= 1
#else
#ifeq ($(SUPPORT_WORKLOAD_ESTIMATION),1)
#SUPPORT_PDVFS ?= 1
#endif
endif

#
# Ensure PDVFS comms. protocol is compatible with RGX GPIO mode
#
ifneq ($(PDVFS_COM),)
ifneq ($(PDVFS_COM),PDVFS_COM_HOST)
SUPPORT_PDVFS ?= 1
SUPPORT_WORKLOAD_ESTIMATION ?= 1
DISABLE_GPU_FREQUENCY_CALIBRATION ?= 1
ifeq ($(PDVFS_COM),PDVFS_COM_AP)
ifneq ($(PVR_GPIO_MODE),)
ifneq ($(PVR_GPIO_MODE),PVR_GPIO_MODE_GENERAL)
# GPIO cannot be used for power monitoring with PDVFS_COM_AP
$(error PDVFS_COM_AP is compatible with PVR_GPIO_MODE_GENERAL only)
endif
endif
endif
endif
endif

ifeq ($(SUPPORT_PDVFS),1)
$(eval $(call BothConfigMake,SUPPORT_PDVFS,1,\
Enabling this feature enables proactive dvfs in the firmware._\
))
$(eval $(call BothConfigC,SUPPORT_PDVFS,1))

$(eval $(call BothConfigC,SUPPORT_PDVFS_IDLE,$(SUPPORT_PDVFS_IDLE),\
This enables idle management in PDVFS._\
))
endif

ifeq ($(SUPPORT_WORKLOAD_ESTIMATION),1)
$(eval $(call BothConfigMake,SUPPORT_WORKLOAD_ESTIMATION,1,\
Enabling this feature enables workload intensity estimation from a workloads_\
characteristics and assigning a deadline to it._\
))
$(eval $(call BothConfigC,SUPPORT_WORKLOAD_ESTIMATION,1))
endif

#
# These specify how PDVFS OPP values are sent by the firmware
#
$(eval $(call BothConfigMake,PDVFS_COM_HOST,1,\
Enables host shared-memory protocol._\
))
$(eval $(call BothConfigC,PDVFS_COM_HOST,1))

$(eval $(call BothConfigMake,PDVFS_COM_AP,2,\
Enables GPIO address protocol._\
))
$(eval $(call BothConfigC,PDVFS_COM_AP,2))

$(eval $(call BothConfigMake,PDVFS_COM_PMC,3,\
Enables GPIO power management controller protocol._\
))
$(eval $(call BothConfigC,PDVFS_COM_PMC,3))

$(eval $(call BothConfigMake,PDVFS_COM_IMG_CLKDIV,4,\
Enables GPIO clock divider control protocol._\
))
$(eval $(call BothConfigC,PDVFS_COM_IMG_CLKDIV,4))

ifeq ($(SUPPORT_STRIP_RENDERING),1)
PDVFS_COM ?= PDVFS_COM_AP
else
PDVFS_COM ?= PDVFS_COM_HOST
endif

$(eval $(call BothConfigMake,PDVFS_COM,$(PDVFS_COM)))
$(eval $(call BothConfigC,PDVFS_COM,$(PDVFS_COM)))

#
# These specify how RGX GPIO port is used by the firmware.
#
$(eval $(call BothConfigMake,PVR_GPIO_MODE_GENERAL,1,\
Enable basic send and receive using GPIO._\
))
$(eval $(call BothConfigC,PVR_GPIO_MODE_GENERAL,1))

$(eval $(call BothConfigMake,PVR_GPIO_MODE_POWMON_PIN,2,\
Enables PMC power monitoring using GPIO._\
))
$(eval $(call BothConfigC,PVR_GPIO_MODE_POWMON_PIN,2))

PVR_GPIO_MODE ?= PVR_GPIO_MODE_GENERAL
$(eval $(call BothConfigMake,PVR_GPIO_MODE,$(PVR_GPIO_MODE)))
$(eval $(call BothConfigC,PVR_GPIO_MODE,$(PVR_GPIO_MODE)))

# If NDK_ROOT is set, SUPPORT_WORKLOAD_ESTIMATION can't be, because the
# ANDROID_WSEGL module uses APIs (binder, gui) which are not in the NDK.
ifeq ($(SUPPORT_WORKLOAD_ESTIMATION),1)
 ifneq ($(PVR_ANDROID_HAS_COMPOSITION_TIMINGS),1)
  ifneq ($(NDK_ROOT),)
   $(error SUPPORT_WORKLOAD_ESTIMATION and NDK_ROOT are incompatible features)
  endif
  ifeq ($(EGL_WSEGL_DIRECTLY_LINKED),1)
   $(error EGL_WSEGL_DIRECTLY_LINKED and SUPPORT_WORKLOAD_ESTIMATION are not supported at the same time)
  endif
 endif
endif

$(eval $(call TunableKernelConfigMake,PVR_HANDLE_BACKEND,idr,\
Specifies the back-end that should be used$(comma) by the Services kernel handle_\
interface$(comma) to allocate handles. The available backends are:_\
* generic (OS agnostic)_\
* idr (Uses the Linux IDR interface)_\
))


$(eval $(call TunableBothConfigC,PVRSRV_ENABLE_PROCESS_STATS,1,\
Enable the collection of Process Statistics in the kernel Server module._\
Feature on by default. Driver_stats summary presented in DebugFS on Linux._\
))

$(eval $(call TunableBothConfigC,PVRSRV_DEBUG_LINUX_MEMORY_STATS,,\
Present Process Statistics memory stats in a more detailed manner to_\
assist with debugging and finding memory leaks (under Linux only)._\
))

$(eval $(call TunableBothConfigC,PVRSRV_ENABLE_PERPID_STATS,,\
Enable the presentation of process statistics in the kernel Server module._\
Feature off by default. \
))

$(eval $(call TunableBothConfigC,SUPPORT_SHARED_SLC,,\
When the SLC is shared the SLC reset is performed by the System layer when_\
calling RGXInitSLC and not the GPU driver. Define this for system layer_\
SLC handling. \
))

# SUPPORT_DMABUF_BRIDGE is set to include the dmabuf.brg in bridge generation
# by default for all Linux based builds.
$(eval $(call TunableBothConfigMake,SUPPORT_DMABUF_BRIDGE,1))

# SUPPORT_USC_BREAKPOINT is set to include the rgxbreakpoint.brg in bridge generation
# and to enable USC breakpoint in FW. Enabled by default for all Linux based builds.
#
SUPPORT_USC_BREAKPOINT ?= 1
$(eval $(call TunableBothConfigMake,SUPPORT_USC_BREAKPOINT,))
$(eval $(call TunableBothConfigC,SUPPORT_USC_BREAKPOINT,,Enable the USC breakpoint support))

# EXCLUDE_CMM_BRIDGE is set to exclude the cmm.brg bridge in
# the Kernel This is disabled by default for release builds.
#
$(eval $(call TunableBothConfigMake,EXCLUDE_CMM_BRIDGE,))
$(eval $(call TunableBothConfigC,EXCLUDE_CMM_BRIDGE,,Disables the cmm bridge))

# EXCLUDE_HTBUFFER_BRIDGE is set to exclude the htbuffer.brg bridge in
# the Kernel This is disabled by default for release builds.
#
$(eval $(call TunableBothConfigMake,EXCLUDE_HTBUFFER_BRIDGE,))
$(eval $(call TunableBothConfigC,EXCLUDE_HTBUFFER_BRIDGE,,Disables the htbuffer bridge))

# EXCLUDE_RGXREGCONFIG_BRIDGE is set to exclude the rgxregconfig.brg bridge in
# the Kernel This is disabled by default for release builds.
#
$(eval $(call TunableBothConfigMake,EXCLUDE_RGXREGCONFIG_BRIDGE,))
$(eval $(call TunableBothConfigC,EXCLUDE_RGXREGCONFIG_BRIDGE,,Disables the RGX regconfig bridge))

# PVRSRV_ENABLE_GPU_MEMORY_INFO is set to enable RI annotation of devmem allocations
# This is enabled by default for debug builds.
#
$(eval $(call TunableBothConfigMake,PVRSRV_ENABLE_GPU_MEMORY_INFO,))
$(eval $(call TunableBothConfigC,PVRSRV_ENABLE_GPU_MEMORY_INFO,,\
Enable Resource Information (RI) debug. This logs details of_\
resource allocations with annotation to help indicate their use._\
))

# PVRSRV_DEBUG_CSW_STATE is set to provide context switch task information
# to FSIM. This is disabled by default for debug and release builds.

ifeq ($(PDUMP),1)
# Force enable TBI interface in PDUMP mode
 override SUPPORT_TBI_INTERFACE :=1
endif

ifeq ($(VIRTUAL_PLATFORM),1)
# Force enable TBI interface for VIRTUAL_PLATFORM
 override SUPPORT_TBI_INTERFACE :=1
endif

$(eval $(call TunableBothConfigC,SUPPORT_TBI_INTERFACE,,\
Enable TBI interface support for firmware._\
))

$(eval $(call TunableBothConfigC,SUPPORT_FIRMWARE_GCOV,,\
Enable gcov support for firmware._\
))


# Safety related options
$(eval $(call TunableBothConfigMake,SUPPORT_TRP,))
$(eval $(call TunableBothConfigC,SUPPORT_TRP,))

$(eval $(call TunableKernelConfigC,RGXFW_SAFETY_WATCHDOG_PERIOD_IN_US,1000000,\
The period in microseconds before the watchdog will trigger a safety_\
event if not reset in time.\
))

ifeq ($(PVRSRV_ENABLE_GPU_MEMORY_INFO),1)
# Increase the default annotation max length to 96 if RI_DEBUG is enabled
override PVR_ANNOTATION_MAX_LEN ?= 96
endif

# Default annotation max length to 63 if no other debug options are specified
$(eval $(call TunableBothConfigC,PVR_ANNOTATION_MAX_LEN,63,\
Defines the max length for PMR$(comma) MemDesc$(comma) Device_\
Memory History and RI debug annotations stored in memory.\
))

$(eval $(call TunableKernelConfigC,SUPPORT_FWLOAD_ON_PROBE,,\
Enable loading of Firmware as part of the driver probe function$(comma)_\
which is the moment an instance of the device gets bound to the driver._\
If the driver fails to load the Firmware at this point$(comma) it will return_\
an error and it will not be possible to open a connection to the device._\
This is a Linux-only feature.\
))

$(eval $(call TunableKernelConfigC,DEBUG_BRIDGE_KM,,\
Enable Services bridge debugging and bridge statistics output_\
))

$(eval $(call TunableBothConfigMake,SUPPORT_DI_BRG_IMPL,1,\
Support OS agnostic Debug Info implementation. This allows to retrieve_\
debugging data previously accessible only via DebugFS with a use of pvrdebug_\
tool._\
))
$(eval $(call TunableBothConfigC,SUPPORT_DI_BRG_IMPL,1,\
Support OS agnostic Debug Info implementation. This allows to retrieve_\
debugging data previously accessible only via DebugFS with a use of pvrdebug_\
tool._\
))

$(eval $(call TunableBothConfigC,PVRSRV_ENABLE_MEMORY_STATS,,\
Enable Memory allocations to be recorded and published via Process Statistics._\
))

$(eval $(call TunableKernelConfigC,PVRSRV_ENABLE_MEMTRACK_STATS_FILE,,\
Enable the memtrack_stats debugfs file when not on an Android platform._\
))

$(eval $(call TunableBothConfigC,PVRSRV_STRICT_COMPAT_CHECK,,\
Enable strict mode of checking all the build options between um & km._\
The driver may fail to load if there is any mismatch in the options._\
))

$(eval $(call TunableBothConfigC,PVR_LINUX_PHYSMEM_MAX_POOL_PAGES,10240,\
Defines how many pages the page cache should hold.))

$(eval $(call TunableBothConfigC,PVR_LINUX_PHYSMEM_MAX_EXCESS_POOL_PAGES,20480,\
We double check if we would exceed this limit if we are below MAX_POOL_PAGES_\
and want to add an allocation to the pool._\
This prevents big allocations being given back to the OS just because they_\
exceed the MAX_POOL_PAGES limit even though the pool is currently empty._\
))

$(eval $(call TunableBothConfigC,PVR_LINUX_PHYSMEM_ZERO_ALL_PAGES,,\
All device memory allocated from the OS via the Rogue driver will be zeroed_\
when this is defined. This may not be necessary in closed platforms where_\
undefined data from previous use in device memory is acceptable._\
This feature may change the performance signature of the drivers memory_\
allocations on some platforms and kernels._\
))

# If target is 32bit
ifeq ($(filter target_arm target_armel target_armhf target_armv7-a \
               target_i686 target_mips target_mips32r6el target_mips32r2el \
               target_x86,$(TARGET_PRIMARY_ARCH)), $(TARGET_PRIMARY_ARCH))
 PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC ?= 1
else
 PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC ?= 0
endif

$(eval $(call TunableKernelConfigC,PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC,PVR_LINUX_PHYSMEM_SUPPRESS_DMA_AC,\
Higher order page requests on Linux use dma_alloc_coherent but on some systems_\
it could return pages from high memory and map those to the vmalloc space._\
Since graphics demand a lot of memory the system could quickly exhaust the_\
vmalloc space. Setting this define will suppress the use of dma_alloc_coherent_\
and fall back to use alloc_pages and not map them to vmalloc space unless_\
requested explicitly by the driver._\
))

$(eval $(call TunableKernelConfigC,PVR_LINUX_PHYSMEM_USE_HIGHMEM_ONLY,,\
GPU buffers are allocated from the highmem region by default._\
Only affects 32bit systems and devices with DMA_BIT_MASK equal to 32._\
))

$(eval $(call TunableKernelConfigC,PVR_PMR_TRANSLATE_UMA_ADDRESSES,,\
Requests for physical addresses from the PMR will translate the addresses_\
retrieved from the PMR-factory from CpuPAddrToDevPAddr. This can be used_\
for systems where the GPU has a different view onto the system memory_\
compared to the CPU._\
))

$(eval $(call TunableBothConfigC,PVR_MMAP_USE_VM_INSERT,,\
If enabled Linux will always use vm_insert_page for CPU mappings._\
vm_insert_page was found to be slower than remap_pfn_range on ARM kernels_\
but guarantees full memory accounting for the process that mapped the memory._\
The slowdown in vm_insert_page is caused by a dcache flush_\
that is only implemented for ARM and a few other architectures._\
This tunable can be enabled to debug memory issues. On x86 platforms_\
we always use vm_insert_page independent of this tunable._\
))

$(eval $(call TunableBothConfigC,PVR_DIRTY_BYTES_FLUSH_THRESHOLD,524288,\
When allocating uncached or write-combine memory we need to invalidate the_\
CPU cache before we can use the acquired pages; also when using cached memory_\
we need to clean/flush the CPU cache before we transfer ownership of the_\
memory to the device. This threshold defines at which number of pages expressed_\
in bytes we want to do a full cache flush instead of invalidating pages one by one._\
Default value is 524288 bytes or 128 pages; ideal value depends on SoC cache size._\
))

$(eval $(call TunableBothConfigC,PVR_LINUX_HIGHORDER_ALLOCATION_THRESHOLD,256,\
Allocate OS pages in 2^(order) chunks if more than this threshold were requested_\
))

PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER ?= 2
$(eval $(call TunableBothConfigC,PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER_NUM,$(PVR_LINUX_PHYSMEM_MAX_ALLOC_ORDER),\
Allocate OS pages in 2^(order) chunks to help reduce duration of large allocations_\
))
ifeq ($(MTK_MINI_PORTING),1)
$(shell echo "#if defined(CONFIG_MTK_ENABLE_GMO)" >> $(CONFIG_KERNEL_H).new)
$(eval $(call TunableBothConfigC,PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD,8192,\
Choose the threshold at which allocation size the driver uses vmalloc instead of_\
kmalloc. On highly fragmented systems large kmallocs can fail because it requests_\
physically contiguous pages. All allocations bigger than this define use vmalloc._\
))
$(shell echo "#else" >> $(CONFIG_KERNEL_H).new)
endif
$(eval $(call TunableBothConfigC,PVR_LINUX_KMALLOC_ALLOCATION_THRESHOLD,16384,\
Choose the threshold at which allocation size the driver uses vmalloc instead of_\
kmalloc. On highly fragmented systems large kmallocs can fail because it requests_\
physically contiguous pages. All allocations bigger than this define use vmalloc._\
))
ifeq ($(MTK_MINI_PORTING),1)
$(shell echo "#endif" >> $(CONFIG_KERNEL_H).new)
endif

$(eval $(call TunableBothConfigMake,SUPPORT_WRAP_EXTMEM,))
$(eval $(call TunableBothConfigC,SUPPORT_WRAP_EXTMEM,,\
This enables support for the Services API function PVRSRVWrapExtMem()_\
which takes a CPU virtual address with size and imports the physical memory_\
behind the CPU virtual addresses into Services for use with the GPU. It_\
returns a memory descriptor that can be used with the usual services_\
interfaces. On Linux the preferred method to import memory into the driver_\
is to use the DMABuf API._\
))

$(eval $(call TunableBothConfigC,PVRSRV_WRAP_EXTMEM_WRITE_ATTRIB_ENABLE,,\
Setting this option enables the write attribute for all the device mappings acquired_\
through the PVRSRVWrapExtMem interface. Otherwise the option is disabled by default._\
))

ifeq ($(PDUMP),1)
$(eval $(call TunableKernelConfigC,PDUMP_PARAM_INIT_STREAM_SIZE,0x200000,\
Default size of pdump param init buffer is 2MB))
$(eval $(call TunableKernelConfigC,PDUMP_PARAM_MAIN_STREAM_SIZE,0x1000000,\
Default size of PDump param main buffer is 16 MB))
$(eval $(call TunableKernelConfigC,PDUMP_PARAM_DEINIT_STREAM_SIZE,0x10000,\
Default size of PDump param deinit buffer is 64KB))
# Default size of PDump param block buffer is 0KB as it is currently not in use
$(eval $(call TunableKernelConfigC,PDUMP_PARAM_BLOCK_STREAM_SIZE,0x0,\
Default size of PDump param block buffer is 0KB))
$(eval $(call TunableKernelConfigC,PDUMP_SCRIPT_INIT_STREAM_SIZE,0x80000,\
Default size of PDump script init buffer is 512KB))
$(eval $(call TunableKernelConfigC,PDUMP_SCRIPT_MAIN_STREAM_SIZE,0x800000,\
Default size of PDump script main buffer is 8MB))
$(eval $(call TunableKernelConfigC,PDUMP_SCRIPT_DEINIT_STREAM_SIZE,0x10000,\
Default size of PDump script deinit buffer is 64KB))
$(eval $(call TunableKernelConfigC,PDUMP_SCRIPT_BLOCK_STREAM_SIZE,0x800000,\
Default size of PDump script block buffer is 8MB))
$(eval $(call TunableKernelConfigC,PDUMP_SPLIT_64BIT_REGISTER_ACCESS,1,\
 Split 64 bit RGX register accesses into two 32 bit))
endif

# Fence Sync build tunables
# Default values dependent on WINDOW_SYSTEM and found in window_system.mk
#
$(eval $(call TunableBothConfigMake,SUPPORT_NATIVE_FENCE_SYNC,$(SUPPORT_NATIVE_FENCE_SYNC)))
$(eval $(call TunableBothConfigC,SUPPORT_NATIVE_FENCE_SYNC,,\
Use the Linux native fence sync back-end with timelines and fences))

$(eval $(call TunableBothConfigMake,SUPPORT_FALLBACK_FENCE_SYNC,))
$(eval $(call TunableBothConfigC,SUPPORT_FALLBACK_FENCE_SYNC,,\
Use Services OS agnostic fallback fence sync back-end with timelines and fences))

$(eval $(call TunableBothConfigC,PVRSRV_STALLED_CCB_ACTION,1,\
This determines behaviour of DDK on detecting that a cCCB_\
has stalled (failed to progress for a number of seconds when GPU is idle):_\
  "" = Output warning message to kernel log only_\
 "1" = Output warning message and additionally try to unblock cCCB by_\
       erroring sync checkpoints on which it is fenced (the value of any_\
       sync prims in the fenced will remain unmodified)_\
))

ifeq ($(SUPPORT_DMA_TRANSFER),1)
 $(eval $(call BothConfigMake,SUPPORT_DMA_TRANSFER,1))
 $(eval $(call BothConfigC,SUPPORT_DMA_TRANSFER,1))
 $(eval $(call TunableKernelConfigC,PVRSRV_DEBUG_DMA,1,\
 Instructs the PVR Services kernel mode driver to produce_\
 additional debug information during the execution of a_\
 DMA transfer such as the physical addresses of the pages_\
 of the source and  destination buffers.))
 ifeq ($(TC_XILINX_DMA),1)
  ifneq ($(call kernel-version-at-least,4,9),true)
  $(error Xilinx DMA requires at least Kernel 4.9)
  endif
 endif
endif

# Fallback and native sync implementations are mutually exclusive because they
# both offer an implementation for the same interface
ifeq ($(SUPPORT_FALLBACK_FENCE_SYNC),1)
ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)
$(error Choose either SUPPORT_NATIVE_FENCE_SYNC=1 or SUPPORT_FALLBACK_FENCE_SYNC=1 but not both)
endif
endif

ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)
PVR_USE_LEGACY_SYNC_H ?= 1

endif

ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)
ifneq ($(KERNEL_VERSION),)
ifeq ($(CHROMIUMOS_KERNEL),1)
ifeq ($(CHROMIUMOS_KERNEL_HAS_DMA_FENCE),1)
KERNEL_COMPATIBLE_WITH_OLD_ANS := 0
else
KERNEL_COMPATIBLE_WITH_OLD_ANS := 1
endif
else
KERNEL_COMPATIBLE_WITH_OLD_ANS := $(shell ( [ $(KERNEL_VERSION) -lt 4 ] || \
[ $(KERNEL_VERSION) -eq 4 -a $(KERNEL_PATCHLEVEL) -lt 6 ] ) && echo 1 || echo 0)
endif
ifneq ($(KERNEL_COMPATIBLE_WITH_OLD_ANS),1)
# DMA fence objects are only supported when using checkpoints
override SUPPORT_DMA_FENCE := 1
endif
KERNEL_COMPATIBLE_WITH_OLD_ANS :=
endif
endif

# This value is needed by ta/3d kick for early command size calculation.
ifeq ($(SUPPORT_NATIVE_FENCE_SYNC),1)
ifeq ($(SUPPORT_DMA_FENCE),)
$(eval $(call KernelConfigC,UPDATE_FENCE_CHECKPOINT_COUNT,2))
else
$(eval $(call KernelConfigC,UPDATE_FENCE_CHECKPOINT_COUNT,1))
endif
else
$(eval $(call KernelConfigC,UPDATE_FENCE_CHECKPOINT_COUNT,1))
endif

$(eval $(call TunableKernelConfigMake,SUPPORT_DMA_FENCE,))

$(eval $(call BothConfigC,PVR_DRM_NAME,"\"pvr\""))



$(eval $(call TunableKernelConfigC,PVRSRV_FORCE_SLOWER_VMAP_ON_64BIT_BUILDS,,\
If enabled$(comma) all kernel mappings will use vmap/vunmap._\
vmap/vunmap is slower than vm_map_ram/vm_unmap_ram and can_\
even have bad peaks taking up to 100x longer than vm_map_ram._\
The disadvantage of vm_map_ram is that it can lead to vmalloc space_\
fragmentation that can lead to vmalloc space exhaustion on 32 bit Linux systems._\
This flag only affects 64 bit Linux builds$(comma) on 32 bit we always default_\
to use vmap because of the described fragmentation problem._\
))
ifeq ($(MTK_MINI_PORTING),1)
$(shell echo "#if defined(__arm64__) || defined(__aarch64__)" >> $(CONFIG_KERNEL_H).new)
endif
$(eval $(call TunableBothConfigC,DEVICE_MEMSETCPY_ALIGN_IN_BYTES,16,\
Sets pointer alignment (in bytes) for PVRSRVDeviceMemSet/Copy._\
This value should reflect memory bus width e.g. if the bus is 64 bits_\
wide this value should be set to 8 bytes (though it's not a hard requirement)._\
))
ifeq ($(MTK_MINI_PORTING),1)
$(shell echo "#else" >> $(CONFIG_KERNEL_H).new)
$(eval $(call TunableBothConfigC,DEVICE_MEMSETCPY_ALIGN_IN_BYTES,8,\
Sets pointer alignment (in bytes) for PVRSRVDeviceMemSet/Copy._\
This value should reflect memory bus width e.g. if the bus is 64 bits_\
wide this value should be set to 8 bytes (though it's not a hard requirement)._\
))
$(shell echo "#endif" >> $(CONFIG_KERNEL_H).new)
endif

$(eval $(call TunableKernelConfigC,PVRSRV_DEBUG_LISR_EXECUTION,,\
Collect information about the last execution of the LISR in order to_\
debug interrupt handling timeouts._\
))

$(eval $(call TunableKernelConfigC,PVRSRV_TIMER_CORRELATION_HISTORY,,\
Collect information about timer correlation data over time._\
))

$(eval $(call TunableKernelConfigC,DISABLE_GPU_FREQUENCY_CALIBRATION,,\
Disable software estimation of the GPU frequency done on the Host and used_\
for timer correlation._\
))

$(eval $(call TunableKernelConfigC,RGX_INITIAL_SLR_HOLDOFF_PERIOD_MS,0,\
Period (in ms) for which any Sync Lockup Recovery (SLR) behaviour should be_\
suppressed following driver load. This can help to avoid any attempted SLR_\
during the boot process._\
))

# Set default CCB sizes
# Key for log2 CCB sizes:
# 13=8K 14=16K 15=32K 16=64K 17=128K
$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TQ3D,14,\
Define the log2 size of the TQ3D client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TQ2D,14,\
Define the log2 size of the TQ2D client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_CDM,13,\
Define the log2 size of the CDM client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TA,15,\
Define the log2 size of the TA client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_3D,16,\
Define the log2 size of the 3D client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_KICKSYNC,13,\
Define the log2 size of the KickSync client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_SIZE_TDM,14,\
Define the log2 size of the TDM client CCB._\
))

# Max sizes (used in CCB grow feature)
$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TQ3D,17,\
Define the log2 max size of the TQ3D client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TQ2D,17,\
Define the log2 max size of the TQ2D client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_CDM,15,\
Define the log2 max size of the CDM client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TA,16,\
Define the log2 max size of the TA client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_3D,17,\
Define the log2 max size of the 3D client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_KICKSYNC,13,\
Define the log2 max size of the KickSync client CCB._\
))

$(eval $(call TunableBothConfigC,PVRSRV_RGX_LOG2_CLIENT_CCB_MAX_SIZE_TDM,17,\
Define the log2 max size of the TDM client CCB._\
))

endif # INTERNAL_CLOBBER_ONLY

export INTERNAL_CLOBBER_ONLY
export TOP
export OUT
export PVR_ARCH
export HWDEFS_ALL_PATHS

MAKE_ETC := -Rr --no-print-directory -C $(TOP) \
		TOP=$(TOP) OUT=$(OUT) HWDEFS_DIR=$(HWDEFS_DIR) \
	        -f build/linux/toplevel.mk

# This must match the default value of MAKECMDGOALS below, and the default
# goal in toplevel.mk
.DEFAULT_GOAL := build

ifeq ($(MAKECMDGOALS),)
MAKECMDGOALS := build
else
# We can't pass autogen to toplevel.mk
MAKECMDGOALS := $(filter-out autogen,$(MAKECMDGOALS))
endif

.PHONY: autogen
autogen:
ifeq ($(INTERNAL_CLOBBER_ONLY),)
	@$(MAKE) -s --no-print-directory -C $(TOP) \
		-f build/linux/prepare_tree.mk
else
	@:
endif

include ../config/help.mk

# This deletes built-in suffix rules. Otherwise the submake isn't run when
# saying e.g. "make thingy.a"
.SUFFIXES:

# Because we have a match-anything rule below, we'll run the main build when
# we're actually trying to remake various makefiles after they're read in.
# These rules try to prevent that
%.mk: ;
Makefile%: ;
Makefile: ;

tags:
	cd $(TOP) ; \
	ctags \
		--recurse=yes \
		--exclude=binary_* \
		--exclude=caches \
		--exclude=docs \
		--exclude=external \
		--languages=C,C++

.PHONY: build kbuild install
build kbuild install: MAKEOVERRIDES :=
build kbuild install: autogen
	@$(if $(MAKECMDGOALS),$(MAKE) $(MAKE_ETC) $(MAKECMDGOALS) $(eval MAKECMDGOALS :=),:)

%: MAKEOVERRIDES :=
%: autogen
	@$(if $(MAKECMDGOALS),$(MAKE) $(MAKE_ETC) $(MAKECMDGOALS) $(eval MAKECMDGOALS :=),:)

endif
