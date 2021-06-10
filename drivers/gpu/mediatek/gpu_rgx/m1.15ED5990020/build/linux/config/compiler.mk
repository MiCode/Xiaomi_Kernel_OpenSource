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

# Check for valid values of $(MULTIARCH).
ifeq ($(strip $(MULTIARCH)),0)
$(error MULTIARCH must be empty to disable multiarch)
endif

define calculate-compiler-preferred-target
 ifeq ($(2),qcc)
  $(1)_compiler_preferred_target := qcc
 else
  # Remove the 'unknown' substring from triple string to behave the same as before clang 8.
  $(1)_compiler_preferred_target := $$(subst --,-,$$(subst unknown,,$$(shell $(2) -dumpmachine)))
  ifeq ($$($(1)_compiler_preferred_target),)
   $$(warning No output from '$(2) -dumpmachine')
   $$(warning Check that the compiler is in your PATH and CROSS_COMPILE is)
   $$(warning set correctly.)
   $$(error Unable to run compiler '$(2)')
  endif
  ifneq ($$(filter %-w64-mingw32,$$($(1)_compiler_preferred_target)),)
   # Use the compiler target name.
  else
   ifneq ($$(filter x86_64-%,$$($(1)_compiler_preferred_target)),)
    $(1)_compiler_preferred_target := x86_64-linux-gnu
   endif
   ifneq ($$(filter i386-% i486-% i586-% i686-%,$$($(1)_compiler_preferred_target)),)
    $(1)_compiler_preferred_target := i386-linux-gnu
   endif
   ifneq ($$(filter aarch64-poky-linux,$$($(1)_compiler_preferred_target)),)
    $(1)_compiler_preferred_target := aarch64-linux-gnu
   endif
   ifneq ($$(filter armv7a-cros-linux-gnueabi armv7l-tizen-linux-gnueabi,$$($(1)_compiler_preferred_target)),)
    $(1)_compiler_preferred_target := arm-linux-gnueabi
   endif
   ifneq ($$(filter arm-linux-android,$$($(1)_compiler_preferred_target)),)
    $(1)_compiler_preferred_target := arm-linux-androideabi
   endif
   ifneq ($$(filter riscv64-buildroot-linux-gnu riscv64-poky-linux,$$($(1)_compiler_preferred_target)),)
    $(1)_compiler_preferred_target := riscv64-linux-gnu
   endif
   ifneq ($$(filter clang%,$(2)),)
    ifeq ($(1),target)
     ifeq (arm-linux-gnueabihf,$$(CROSS_TRIPLE))
      $(1)_compiler_preferred_target := arm-linux-gnueabihf
     endif
     ifeq (arm-linux-gnueabi,$$(CROSS_TRIPLE))
      $(1)_compiler_preferred_target := arm-linux-gnueabi
     endif
    endif
   endif
  endif
 endif
endef

define cross-compiler-name
 ifeq ($$(_CLANG),true)
  ifneq ($(strip $(2)),)
   ifeq ($(1):$(CROSS_TRIPLE),_cc_secondary:mips64el-linux-android)
    $(1) := $(3) -target mipsel-linux-android -Qunused-arguments
   else
    $(1) := $(3) -target $$(patsubst %-,%,$$(notdir $(2))) -Qunused-arguments
   endif
  else
   $(1) := $(3) -Qunused-arguments
  endif
 else
  ifeq ($$(origin CC),file)
   $(1) := $(2)$(3)
  else
   $(1) := $(3)
  endif
 endif
endef

# Work out the host compiler architecture
$(eval $(call calculate-compiler-preferred-target,host,$(HOST_CC)))

ifeq ($(host_compiler_preferred_target),x86_64-linux-gnu)
 HOST_PRIMARY_ARCH := host_x86_64
 HOST_32BIT_ARCH   := host_i386
 HOST_FORCE_32BIT  := -m32
else
ifeq ($(host_compiler_preferred_target),i386-linux-gnu)
 HOST_PRIMARY_ARCH := host_i386
 HOST_32BIT_ARCH   := host_i386
else
ifeq ($(host_compiler_preferred_target),arm-linux-gnueabi)
 HOST_PRIMARY_ARCH := host_armel
 HOST_32BIT_ARCH   := host_armel
else
ifeq ($(host_compiler_preferred_target),arm-linux-gnueabihf)
 HOST_PRIMARY_ARCH := host_armhf
 HOST_32BIT_ARCH   := host_armhf
else
ifeq ($(host_compiler_preferred_target),aarch64-linux-gnu)
 HOST_PRIMARY_ARCH := host_aarch64
 HOST_32BIT_ARCH   := host_armhf
else
 $(error Unknown host compiler target architecture $(host_compiler_preferred_target))
endif
endif
endif
endif
endif

# We set HOST_ALL_ARCH this way, as the host architectures may be overridden
# on the command line.
ifeq ($(HOST_PRIMARY_ARCH),$(HOST_32BIT_ARCH))
 HOST_ALL_ARCH := $(HOST_PRIMARY_ARCH)
else
 HOST_ALL_ARCH := $(HOST_PRIMARY_ARCH) $(HOST_32BIT_ARCH)
endif

# Workaround our lack of support for non-Linux HOST_CCs
ifneq ($(HOST_CC_IS_LINUX),1)
 $(warning $$(HOST_CC) is non-Linux. Trying to work around.)
 override HOST_CC := $(HOST_CC) -D__linux__
 $(eval $(call BothConfigMake,HOST_CC,$(HOST_CC)))
endif

$(eval $(call BothConfigMake,HOST_PRIMARY_ARCH,$(HOST_PRIMARY_ARCH)))
$(eval $(call BothConfigMake,HOST_32BIT_ARCH,$(HOST_32BIT_ARCH)))
$(eval $(call BothConfigMake,HOST_FORCE_32BIT,$(HOST_FORCE_32BIT)))
$(eval $(call BothConfigMake,HOST_ALL_ARCH,$(HOST_ALL_ARCH)))

TARGET_ALL_ARCH :=
TARGET_PRIMARY_ARCH :=
TARGET_SECONDARY_ARCH :=

# Work out the target compiler cross triple, and include the corresponding
# compilers/*.mk file, which sets TARGET_PRIMARY_ARCH and
# TARGET_SECONDARY_ARCH for that compiler.
#
compilers := ../config/compilers
define include-compiler-file
 ifeq ($(strip $(1)),)
  $$(error empty arg passed to include-compiler-file)
 endif
 ifeq ($$(wildcard $$(compilers)/$(1).mk),)
  $$(warning ******************************************************)
  $$(warning Compiler target '$(1)' not recognised)
  $$(warning (missing $$(compilers)/$(1).mk file))
  $$(warning ******************************************************)
  $$(error Compiler '$(1)' not recognised)
 endif
 include $$(compilers)/$(1).mk
endef

# Check the kernel cross compiler to work out which architecture it targets.
# We can then tell if CROSS_COMPILE targets a different architecture.
ifneq ($(origin KERNEL_CROSS_COMPILE),undefined)
 # First, calculate the value of KERNEL_CROSS_COMPILE as it would be seen by
 # the main build, so we can check it here in the config stage.
 $(call one-word-only,KERNEL_CROSS_COMPILE)
 _kernel_cross_compile := $(if $(filter undef,$(KERNEL_CROSS_COMPILE)),,$(KERNEL_CROSS_COMPILE))
 # We can take shortcuts with KERNEL_CROSS_COMPILE, as we don't want to
 # respect CC and we don't support clang in that part currently.
 _kernel_cross_compile := $(_kernel_cross_compile)gcc
 # Then check the compiler.
 $(eval $(call calculate-compiler-preferred-target,target,$(_kernel_cross_compile)))
 $(eval $(call include-compiler-file,$(target_compiler_preferred_target)))
 _kernel_primary_arch := $(TARGET_PRIMARY_ARCH)
else
 # We can take shortcuts with KERNEL_CROSS_COMPILE, as we don't want to
 # respect CC and we don't support clang in that part currently.
 _kernel_cross_compile := $(CROSS_COMPILE)gcc
 # KERNEL_CROSS_COMPILE will be the same as CROSS_COMPILE, so we don't need
 # to do the compatibility check.
 _kernel_primary_arch :=
endif

$(eval $(call cross-compiler-name,_cc,$(CROSS_COMPILE),$(CC)))
$(eval $(call cross-compiler-name,_cc_secondary,$(if $(CROSS_COMPILE_SECONDARY),$(CROSS_COMPILE_SECONDARY),$(CROSS_COMPILE)),$(CC_SECONDARY)))
$(eval $(call calculate-compiler-preferred-target,target,$(_cc)))
$(eval $(call include-compiler-file,$(target_compiler_preferred_target)))

ifneq ($(SUPPORT_ANDROID_PLATFORM),1)
ifeq ($(MULTIARCH),1)
 ifneq ($(MAKECMDGOALS),kbuild)
  ifneq ($(COMPONENTS),)
   $(eval $(call calculate-compiler-preferred-target,target_secondary,$(_cc_secondary)))
   ifneq ($(target_compiler_preferred_target),$(target_secondary_compiler_preferred_target))
    $(eval $(call include-compiler-file,$(target_secondary_compiler_preferred_target)))

    ifeq ($(TARGET_SECONDARY_ARCH),)
     $(error $(CROSS_COMPILE_SECONDARY) not supported for MULTIARCH builds)
    endif
   endif
  endif
 endif
endif
endif

define remap-arch
$(if $(INTERNAL_ARCH_REMAP_$(1)),$(INTERNAL_ARCH_REMAP_$(1)),$(1))
endef

# Remap 'essentially compatible' architectures so the KM vs UM check
# isn't too strict. These mixtures are widely supported.
INTERNAL_ARCH_REMAP_target_armhf := target_armv7-a
INTERNAL_ARCH_REMAP_target_armel := target_armv7-a
INTERNAL_ARCH_REMAP_target_mips32r2el := target_mips32el
INTERNAL_ARCH_REMAP_target_mips32r6el := target_mips32el

# Sanity check: if KERNEL_CROSS_COMPILE was set, it has to target the same
# architecture as CROSS_COMPILE.
ifneq ($(_kernel_primary_arch),)
 ifneq ($(call remap-arch,$(TARGET_PRIMARY_ARCH)),$(call remap-arch,$(_kernel_primary_arch)))
  $(warning ********************************************************)
  $(warning Error: Kernel and user-mode cross compilers build for)
  $(warning different targets)
  $(warning $(space)$(space)CROSS_COMPILE=$(CROSS_COMPILE))
  $(warning $(space)$(space)$(space)builds for $(TARGET_PRIMARY_ARCH))
  $(warning $(space)$(space)KERNEL_CROSS_COMPILE=$(KERNEL_CROSS_COMPILE))
  $(warning $(space)$(space)$(space)builds for $(_kernel_primary_arch))
  $(warning ********************************************************)
  $(error Mismatching kernel and user-mode cross compilers)
 endif
endif

ifneq ($(MULTIARCH),32only)
TARGET_ALL_ARCH += $(TARGET_PRIMARY_ARCH)
endif
ifneq ($(MULTIARCH),64only)
TARGET_ALL_ARCH += $(TARGET_SECONDARY_ARCH)
endif

$(eval $(call BothConfigMake,TARGET_PRIMARY_ARCH,$(TARGET_PRIMARY_ARCH)))
$(eval $(call BothConfigMake,TARGET_SECONDARY_ARCH,$(TARGET_SECONDARY_ARCH)))
$(eval $(call BothConfigMake,TARGET_ALL_ARCH,$(TARGET_ALL_ARCH)))
$(eval $(call BothConfigMake,TARGET_FORCE_32BIT,$(TARGET_FORCE_32BIT)))

$(info ******* Multiarch build: $(if $(MULTIARCH),yes,no))
$(info ******* Primary arch:    $(if $(TARGET_PRIMARY_ARCH),$(TARGET_PRIMARY_ARCH),none))
$(info ******* Secondary arch:  $(if $(TARGET_SECONDARY_ARCH),$(TARGET_SECONDARY_ARCH),none))
$(info ******* PVR arch:        $(PVR_ARCH))
$(info ******* HWDefs:          $(HWDEFS_DIR))
$(info ******* HWDefs (all):    $(HWDEFS_ALL_PATHS))
$(info ******* Host OS:         $(HOST_OS))
$(info ******* Target OS:       $(TARGET_OS))

ifeq ($(SUPPORT_NEUTRINO_PLATFORM),)
 # Find the paths to libgcc for the primary and secondary architectures.
 LIBGCC := $(shell $(_cc) -print-libgcc-file-name)
 LIBGCC_SECONDARY := $(shell $(_cc_secondary) $(TARGET_FORCE_32BIT) -print-libgcc-file-name)

 # Android clang toolchain drivers cannot find libgcc.a for various triples.
 # We will use a fixed path to the last supported version (4.9.x) of GCC.
 #
 ifeq ($(SUPPORT_ANDROID_PLATFORM)$(SUPPORT_ARC_PLATFORM),1)
  ifeq ($(_CLANG),true)
   LIBGCC_PREBUILT_PATH := $(ANDROID_ROOT)/prebuilts/gcc/linux-x86
   ifeq ($(filter-out arm%,$(ARCH)),)
    LIBGCC := $(LIBGCC_PREBUILT_PATH)/aarch64/aarch64-linux-android-4.9/lib/gcc/aarch64-linux-android/4.9.x/libgcc.a
    LIBGCC_SECONDARY := $(LIBGCC_PREBUILT_PATH)/arm/arm-linux-androideabi-4.9/lib/gcc/arm-linux-androideabi/4.9.x/libgcc.a
   else
    LIBGCC := $(LIBGCC_PREBUILT_PATH)/x86/x86_64-linux-android-4.9/lib/gcc/x86_64-linux-android/4.9.x/libgcc.a
    LIBGCC_SECONDARY := $(LIBGCC_PREBUILT_PATH)/x86/x86_64-linux-android-4.9/lib/gcc/x86_64-linux-android/4.9.x/32/libgcc.a
   endif
  endif
 endif
endif
