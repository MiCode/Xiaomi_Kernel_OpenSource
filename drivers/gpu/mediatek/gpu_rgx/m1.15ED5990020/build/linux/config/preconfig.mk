########################################################################### ###
#@File
#@Title         Set up configuration required by build-directory Makefiles
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

# NOTE: Don't put anything in this file that isn't strictly required
# by the build-directory Makefiles. It should go in core.mk otherwise.

TOP ?= $(abspath ../../..)

# Some miscellaneous things to make comma substitutions easier.
apos := '#'
comma := ,
empty :=
space := $(empty) $(empty)
define newline


endef

ifneq ($(words $(TOP)),1)
$(warning This source tree is located in a path which contains whitespace,)
$(warning which is not supported.)
$(warning )
$(warning $(space)The root is: $(TOP))
$(warning )
$(error Whitespace found in $$(TOP))
endif

$(call directory-must-exist,$(TOP))

define ValidateValues
_supported_values := $(2)
_values := $$(subst $$(comma),$$(space),$$($(1)))
_unrecognised_values := $$(strip $$(filter-out $$(_supported_values),$$(_values)))
ifneq ($$(_unrecognised_values),)
$$(warning *** Unrecognised value(s): $$(_unrecognised_values))
$$(warning *** $(1) was set via: $(origin $(1)))
$$(error Supported values are: $$(_supported_values))
endif
endef

ifeq ($(SUPPORT_NEUTRINO_PLATFORM),1)
include ../common/neutrino/preconfig_neutrino.mk
_CC := $(CC)
else

CC_CHECK  := ../tools/cc-check.sh
CHMOD     := chmod

# Return true if the parameter is a variable containing the
# name of a CLang based C compiler, else return false.
define compiler-is-clang
$(shell $(CC_CHECK) --clang --cc "$($(1))")
endef

# If the first parameter is an undefined variable, or a variable
# with a default value (e.g. CC or AR), set the variable to the
# second parameter, and export it.
define cond-set-and-export
 ifneq ($(filter $(origin $(1)),default undefined),)
  export $(1) := $(2)
 endif
endef

PVR_BUILD_DIR := $(notdir $(abspath .))
ifneq ($(PVR_BUILD_DIR),$(patsubst %_android,%,$(PVR_BUILD_DIR))) # Android build
 is_android_build := true
 include ../common/android/platform_version.mk
 ifeq ($(MTK_MINI_PORTING),1)
  prefer_prebuilt_host_toolchains ?= 1
 endif
 # KERNEL_CC is configured in build/linux/common/android/features.mk
 # for android builds.
 prefer_clang_kbuild := false
 ifneq ($(USE_CLANG),0)
  prefer_clang := true
 else
  $(info WARNING: USE_CLANG=0 is deprecated for Android builds)
 endif
else
 is_android_build := false
 ifeq ($(USE_CLANG),1)
  prefer_clang := true
  ifeq ($(KERNEL_CC),)
   prefer_clang_kbuild := true
  else
   prefer_clang_kbuild := $(call compiler-is-clang,KERNEL_CC)
  endif
 endif
endif

CROSS_TRIPLE := $(patsubst %-,%,$(notdir $(CROSS_COMPILE)))

define clangify
 ifneq ($$(strip $$(CROSS_TRIPLE)),)
  _$(1) := $$($(1)) -target $$(patsubst %-,%,$$(CROSS_TRIPLE)) -Qunused-arguments
 else
  _$(1) := $$($(1)) -Qunused-arguments
 endif
endef

# GNU Make has builtin values for CC/CXX which we don't want to trust. This
# is because $(CROSS_COMPILE)$(CC) doesn't always expand to a cross compiler
# toolchain binary name (e.g. most toolchains have 'gcc' but not 'cc').

CLANG ?= clang
ifeq ($(origin CC),default)
 ifeq ($(prefer_clang),true)
  export CC := $(CLANG)
  _CLANG := true
  $(eval $(call clangify,CC))
 else
  CC  := gcc
  _CC := $(CROSS_COMPILE)gcc
 endif
else
 _CLANG := $(call compiler-is-clang,CC)
 ifeq ($(_CLANG),true)
  $(eval $(call clangify,CC))
 else
  _CC := $(CC)
 endif
endif

CLANG_CXX ?= clang++
ifeq ($(origin CXX),default)
 ifeq ($(prefer_clang),true)
  export CXX := $(CLANG_CXX)
 else
  CXX := g++
 endif
endif

ifeq ($(USE_LTO),1)
 ifeq ($(is_android_build),false)
  ifeq ($(call compiler-is-clang,CC),true)
   $(eval $(call cond-set-and-export,AR,llvm-ar))
   $(eval $(call cond-set-and-export,RANLIB,llvm-ranlib))
  else
   $(eval $(call cond-set-and-export,AR,$(CROSS_COMPILE)gcc-ar))
   $(eval $(call cond-set-and-export,RANLIB,$(CROSS_COMPILE)gcc-ranlib))
  endif
 endif
endif

CLANG_LD ?= ld.lld
CLANG_NM ?= llvm-nm
CLANG_OBJCOPY ?= llvm-objcopy
ifeq ($(prefer_clang_kbuild),true)
 ifeq ($(KERNEL_CC),)
  export KERNEL_CC := $(CLANG)
 endif
 export KERNEL_LD := $(CLANG_LD)
 export KERNEL_NM := $(CLANG_NM)
 export KERNEL_OBJCOPY := $(CLANG_OBJCOPY)
endif

CC_SECONDARY ?= $(CC)
CXX_SECONDARY ?= $(CXX)
ifeq ($(prefer_clang),true)
 export CC_SECONDARY
 export CXX_SECONDARY
endif

ifeq ($(USE_LTO),1)
 ifeq ($(is_android_build),false)
  ifeq ($(call compiler-is-clang,CC_SECONDARY),true)
   $(eval $(call cond-set-and-export,AR_SECONDARY,llvm-ar))
   $(eval $(call cond-set-and-export,RANLIB_SECONDARY,llvm-ranlib))
  else
   $(eval $(call cond-set-and-export,AR_SECONDARY,$(if $(CROSS_COMPILE_SECONDARY),$(CROSS_COMPILE_SECONDARY),$(CROSS_COMPILE))gcc-ar))
   $(eval $(call cond-set-and-export,RANLIB_SECONDARY,$(if $(CROSS_COMPILE_SECONDARY),$(CROSS_COMPILE_SECONDARY),$(CROSS_COMPILE))gcc-ranlib))
  endif
 endif
endif

ifeq ($(prefer_clang),true)
 ifeq ($(HOST_CC),)
  ifeq ($(MTK_MINI_PORTING),1)
   export HOST_CC := $(CC) -target x86_64-linux-gnu
  else
   export HOST_CC := /usr/bin/clang
  endif
 endif
 ifeq ($(HOST_CXX),)
  ifeq ($(MTK_MINI_PORTING),1)
   export HOST_CXX := $(CXX) -target x86_64-linux-gnu
  else
   export HOST_CXX := /usr/bin/clang++
  endif
 endif
else
 HOST_CC ?= gcc
endif

ifeq ($(USE_LTO),1)
 ifeq ($(is_android_build),false)
  ifeq ($(call compiler-is-clang,HOST_CC),true)
   $(eval $(call cond-set-and-export,HOST_AR,llvm-ar))
   $(eval $(call cond-set-and-export,HOST_RANLIB,llvm-ranlib))
  else
   $(eval $(call cond-set-and-export,HOST_AR,gcc-ar))
   $(eval $(call cond-set-and-export,HOST_RANLIB,gcc-ranlib))
  endif
 endif
endif

# Work out if we are targeting ARM before we start tweaking _CC.
TARGETING_AARCH64 := $(shell \
 $(_CC) -dM -E - </dev/null | grep -q __aarch64__ && echo 1)

TARGETING_ARM := $(shell \
 $(_CC) -dM -E - </dev/null | grep __arm__ >/dev/null 2>&1 && echo 1)

TARGETING_MIPS := $(shell \
 $(_CC) -dM -E - </dev/null | grep __mips__ >/dev/null 2>&1 && echo 1)

HOST_CC_IS_LINUX := $(shell \
 $(HOST_CC) -dM -E - </dev/null | grep __linux__ >/dev/null 2>&1 && echo 1)

ifneq ($(strip $(KERNELDIR)),)
include ../config/kernel_version.mk
endif

# The user didn't set CROSS_COMPILE. There's probably nothing wrong
# with that, but we'll let them know anyway.
#
ifeq ($(origin CROSS_COMPILE), undefined)
$(warning CROSS_COMPILE is not set. Target components will be built with the host compiler)
endif

endif # Neutrino

define calculate-os
 ifeq ($(2),qcc)
  $(1)_OS := neutrino
 else
  compiler_dumpmachine := $$(subst --,-,$$(shell $(2) -dumpmachine))
  ifeq ($$(compiler_dumpmachine),)
   $$(warning No output from '$(2) -dumpmachine')
   $$(warning Check that the compiler is in your PATH and CROSS_COMPILE is)
   $$(warning set correctly.)
   $$(error Unable to run compiler '$(2)')
  endif

  triplet_word_list := $$(subst -, ,$$(compiler_dumpmachine))
  triplet_word_list_length := $$(words $$(triplet_word_list))
  ifeq ($$(triplet_word_list_length),4)
   triplet_vendor := $$(word 2,$$(triplet_word_list))
   triplet_os_list := $$(wordlist 3,$$(triplet_word_list_length),$$(triplet_word_list))
  else ifeq ($$(triplet_word_list_length),3)
   triplet_vendor := unknown
   triplet_os_list := $$(wordlist 2,$$(triplet_word_list_length),$$(triplet_word_list))
  else
   $$(error Unsupported compiler: $(2))
  endif

  triplet_os := $$(subst $$(space),-,$$(triplet_os_list))
  ifeq ($$(triplet_os),linux-android)
   $(1)_OS := android
  else ifeq ($$(triplet_os),poky-linux)
   $(1)_OS := poky
  else ifeq ($$(triplet_os),w64-mingw32)
   $(1)_OS := windows
  else ifneq ($$(findstring linux-gnu,$$(triplet_os)),)
   ifneq ($$(filter buildroot cros tizen,$$(triplet_vendor)),)
    $(1)_OS := $$(triplet_vendor)
   else ifneq ($$(filter none pc unknown,$$(triplet_vendor)),)
    $(1)_OS := linux
   else
    $$(warning Unsupported compiler vendor: $$(triplet_vendor))
    $$(warning Assuming $(1) is a standard Linux distro)
    $(1)_OS := linux
   endif
  else
   $$(warning Could not determine $(1)_OS so assuming Linux)
   $(1)_OS := linux
  endif
 endif
endef

$(eval $(call calculate-os,HOST,$(HOST_CC)))

ifeq ($(PVR_BUILD_DIR),integrity)
 TARGET_OS := integrity
else ifneq ($(PVR_BUILD_DIR),$(patsubst %_arc,%,$(PVR_BUILD_DIR)))
 TARGET_OS := arc
else
 $(eval $(call calculate-os,TARGET,$(_CC)))
endif

define is-host-os
$(if $(HOST_OS),$(if $(filter $(1),$(HOST_OS)),true),$(error HOST_OS not set))
endef

define is-not-host-os
$(if $(HOST_OS),$(if $(filter-out $(1),$(HOST_OS)),true),$(error HOST_OS not set))
endef

define is-target-os
$(if $(TARGET_OS),$(if $(filter $(1),$(TARGET_OS)),true),$(error TARGET_OS not set))
endef

define is-not-target-os
$(if $(TARGET_OS),$(if $(filter-out $(1),$(TARGET_OS)),true),$(error TARGET_OS not set))
endef

ifeq ($(call is-target-os,buildroot),true)
 SYSROOT ?= $(shell $(_CC) -print-sysroot)
else ifeq ($(call is-target-os,poky),true)
 _OPTIM := $(lastword $(filter -O%,$(CFLAGS)))
 ifneq ($(_OPTIM),)
  OPTIM ?= $(_OPTIM)

  # Filter out any -O flags in case a platform Makefile makes use of these
  # variables
  override CFLAGS := $(filter-out -O%,$(CFLAGS))
  override CXXFLAGS := $(filter-out -O%,$(CXXFLAGS))
 endif
endif

# The user is trying to set one of the old SUPPORT_ options on the
# command line or in the environment. This isn't supported any more
# and will often break the build. The user is generally only trying
# to remove a component from the list of targets to build, so we'll
# point them at the new way of doing this.
define sanity-check-support-option-origin
ifeq ($$(filter undefined file,$$(origin $(1))),)
$$(warning *** Setting $(1) via $$(origin $(1)) is deprecated)
$$(error If you are trying to disable a component, use e.g. EXCLUDED_APIS="opengles1 opengl")
endif
endef
$(foreach _o,SYS_CFLAGS SYS_CXXFLAGS SYS_INCLUDES SYS_EXE_LDFLAGS SYS_LIB_LDFLAGS,$(eval $(call sanity-check-support-option-origin,$(_o))))

# Check for words in EXCLUDED_APIS that aren't understood by the
# common/apis/*.mk files. This should be kept in sync with all the tests on
# EXCLUDED_APIS in those files
ifeq ($(MTK_MINI_PORTING),1)
_excludable_apis := camerahal cldnn imgdnn nnhal composerhal hwperftools memtrackhal opencl opengl opengles1 opengles3 openglsc2 rogue2d scripts sensorhal servicestools thermalhal unittests vulkan
else
_excludable_apis := camerahal composerhal hwperftools memtrackhal opencl opengl opengles1 opengles3 openglsc2 rogue2d scripts sensorhal servicestools thermalhal unittests vulkan
endif
_excluded_apis := $(subst $(comma),$(space),$(EXCLUDED_APIS))

_unrecognised := $(strip $(filter-out $(_excludable_apis),$(_excluded_apis)))
ifneq ($(_unrecognised),)
$(warning *** Ignoring unrecognised entries in EXCLUDED_APIS: $(_unrecognised))
$(warning *** EXCLUDED_APIS was set via $(origin EXCLUDED_APIS) to: $(EXCLUDED_APIS))
$(warning *** Excludable APIs are: $(_excludable_apis))
endif

override EXCLUDED_APIS := $(filter $(_excludable_apis), $(_excluded_apis))

# Some targets don't need information about any modules. If we only specify
# these targets on the make command line, set INTERNAL_CLOBBER_ONLY to
# indicate that toplevel.mk shouldn't read any makefiles
CLOBBER_ONLY_TARGETS := clean clobber help install
INTERNAL_CLOBBER_ONLY :=
ifneq ($(strip $(MAKECMDGOALS)),)
INTERNAL_CLOBBER_ONLY := \
$(if \
 $(strip $(foreach _cmdgoal,$(MAKECMDGOALS),\
          $(if $(filter $(_cmdgoal),$(CLOBBER_ONLY_TARGETS)),,x))),,true)
endif

# No need for BVNC information for clobber-only build
ifneq ($(INTERNAL_CLOBBER_ONLY),true)

# If RGX_BVNC is not defined a valid PVR_ARCH has to be specified
ifeq ($(RGX_BVNC),)
 ifneq ($(PVR_ARCH),)
  _supported_pvr_archs := rogue volcanic
  $(eval $(call ValidateValues,PVR_ARCH,$(_supported_pvr_archs)))
  override HWDEFS_DIR := $(TOP)/hwdefs/$(PVR_ARCH)
 endif
else
 ifneq ($(PVR_ARCH),)
  $(warning PVR_ARCH ($(PVR_ARCH)) is specified when RGX_BVNC ($(RGX_BVNC)) is also specified - ignoring PVR_ARCH)
 endif
# Extract the BNC config name
RGX_BNC_SPLIT := $(subst .,$(space) ,$(RGX_BVNC))
RGX_BNC := $(word 1,$(RGX_BNC_SPLIT)).V.$(word 3,$(RGX_BNC_SPLIT)).$(word 4,$(RGX_BNC_SPLIT))

HWDEFS_DIR_ROGUE := $(TOP)/hwdefs/rogue
HWDEFS_DIR_VOLCANIC := $(TOP)/hwdefs/volcanic

ALL_KM_BVNCS_ROGUE := \
 $(patsubst rgxcore_km_%.h,%,\
  $(notdir $(shell ls -v $(HWDEFS_DIR_ROGUE)/km/cores/rgxcore_km_*.h 2> /dev/null)))
ALL_KM_BVNCS_VOLCANIC := \
 $(patsubst rgxcore_km_%.h,%,\
  $(notdir $(shell ls -v $(HWDEFS_DIR_VOLCANIC)/km/cores/rgxcore_km_*.h 2> /dev/null)))
ALL_KM_BVNCS := $(ALL_KM_BVNCS_ROGUE) $(ALL_KM_BVNCS_VOLCANIC)

ifneq ($(filter $(RGX_BVNC),$(ALL_KM_BVNCS_ROGUE)),)
 override PVR_ARCH := rogue
 override HWDEFS_DIR := $(HWDEFS_DIR_ROGUE)
else ifneq ($(filter $(RGX_BVNC),$(ALL_KM_BVNCS_VOLCANIC)),)
 override PVR_ARCH := volcanic
 override HWDEFS_DIR := $(HWDEFS_DIR_VOLCANIC)
else
 $(error Error: Invalid Kernel core RGX_BVNC=$(RGX_BVNC). \
  $(if $(ALL_KM_BVNCS_ROGUE),$(newline)$(newline)Valid Rogue Kernel core BVNCs:$(newline) \
   $(subst $(space),$(newline)$(space),$(ALL_KM_BVNCS_ROGUE))) \
  $(if $(ALL_KM_BVNCS_VOLCANIC),$(newline)$(newline)Valid Volcanic Kernel core BVNCs:$(newline) \
   $(subst $(space),$(newline)$(space),$(ALL_KM_BVNCS_VOLCANIC))))
endif

override HWDEFS_ALL_PATHS := $(HWDEFS_DIR) $(HWDEFS_DIR)/$(RGX_BNC) $(HWDEFS_DIR)/$(RGX_BNC)/isa $(HWDEFS_DIR)/km

# Check if BVNC core file exist
RGX_BVNC_CORE_KM := $(HWDEFS_DIR)/km/cores/rgxcore_km_$(RGX_BVNC).h
RGX_BVNC_CORE_KM_HEADER := \"cores/rgxcore_km_$(RGX_BVNC).h\"
# "rgxcore_km_$(RGX_BVNC).h"
ifeq ($(wildcard $(RGX_BVNC_CORE_KM)),)
$(error The file $(RGX_BVNC_CORE_KM) does not exist. \
   Valid BVNCs: $(ALL_KM_BVNCS))
endif

# Check BNC config version
ALL_KM_BNCS := \
 $(patsubst rgxconfig_km_%.h,%,\
   $(notdir $(shell ls -v $(HWDEFS_DIR)/km/configs/rgxconfig_km_*.h)))
ifeq ($(filter $(RGX_BNC),$(ALL_KM_BNCS)),)
$(error Error: Invalid Kernel config RGX_BNC=$(RGX_BNC). \
   Valid Kernel config BNCs: $(subst $(space),$(comma)$(space),$(ALL_KM_BNCS)))
endif

# Check if BNC config file exist
RGX_BNC_CONFIG_KM := $(HWDEFS_DIR)/km/configs/rgxconfig_km_$(RGX_BNC).h
RGX_BNC_CONFIG_KM_HEADER := \"configs/rgxconfig_km_$(RGX_BNC).h\"
#"rgxcore_km_$(RGX_BNC).h"
ifeq ($(wildcard $(RGX_BNC_CONFIG_KM)),)
$(error The file $(RGX_BNC_CONFIG_KM) does not exist. \
   Valid BNCs: $(ALL_KM_BNCS))
endif
endif

endif
