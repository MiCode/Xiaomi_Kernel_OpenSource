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

# from-one-* recipes make a thing from one source file, so they use $<. Others
# use $(MODULE_something) instead of $^

# We expect that MODULE_*FLAGS contains all the flags we need, including the
# flags for all modules (like $(ALL_CFLAGS) and $(ALL_HOST_CFLAGS)), and
# excluding flags for include search dirs or for linking libraries. The
# exceptions are ALL_EXE_LDFLAGS and ALL_LIB_LDFLAGS, since they depend on the
# type of thing being linked, so they appear in the commands below

define host-o-from-one-c
$(if $(V),,@echo "  HOST_CC " $(call relative-to-top,$<))
$(MODULE_CC) -MD -MP -MF $(patsubst %.o,%.d,$@) -c $(MODULE_CFLAGS) \
	$(MODULE_INCLUDE_FLAGS) -include $(CONFIG_H) $< -o $@
endef

define target-o-from-one-c
$(if $(V),,@echo "  CC      " $(call relative-to-top,$<))
$(MODULE_CC) -MD -MP -MF $(patsubst %.o,%.d,$@) -c $(MODULE_CFLAGS) \
	$(MODULE_INCLUDE_FLAGS) -include $(CONFIG_H) $< -o $@
endef

define host-o-from-one-cxx
$(if $(V),,@echo "  HOST_CXX" $(call relative-to-top,$<))
$(MODULE_CXX) -MD -MP -MF $(patsubst %.o,%.d,$@) -c $(MODULE_CXXFLAGS) \
	$(MODULE_INCLUDE_FLAGS) -include $(CONFIG_H) $< -o $@
endef

define target-o-from-one-cxx
$(if $(V),,@echo "  CXX     " $(call relative-to-top,$<))
$(MODULE_CXX) -MD -MP -MF $(patsubst %.o,%.d,$@) -c $(MODULE_CXXFLAGS) \
	$(MODULE_INCLUDE_FLAGS) -include $(CONFIG_H) $< -o $@
endef

define host-executable-from-o
$(if $(V),,@echo "  HOST_LD " $(call relative-to-top,$@))
$(MODULE_CC) $(MODULE_LDFLAGS) \
	-o $@ $(sort $(MODULE_ALL_OBJECTS)) $(MODULE_LIBRARY_DIR_FLAGS) \
	$(MODULE_LIBRARY_FLAGS)
endef

define host-executable-cxx-from-o
$(if $(V),,@echo "  HOST_LD " $(call relative-to-top,$@))
$(MODULE_CXX) $(MODULE_LDFLAGS) \
	-o $@ $(sort $(MODULE_ALL_OBJECTS)) $(MODULE_LIBRARY_DIR_FLAGS) \
	$(MODULE_LIBRARY_FLAGS)
endef

define target-executable-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(MODULE_CC) \
	$(MODULE_TARGET_VARIANT_TYPE) $(MODULE_LDFLAGS) -o $@ \
	$(MODULE_EXE_CRTBEGIN) $(MODULE_ALL_OBJECTS) $(MODULE_EXE_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) \
	$(MODULE_EXE_LDFLAGS)
endef

define target-executable-cxx-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(MODULE_CXX) \
	$(MODULE_TARGET_VARIANT_TYPE) $(MODULE_LDFLAGS) -o $@ \
	$(MODULE_EXE_CRTBEGIN) $(MODULE_ALL_OBJECTS) $(MODULE_EXE_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) \
	$(MODULE_EXE_LDFLAGS)
endef

define target-shared-library-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(MODULE_CC) -shared -Wl,-Bsymbolic \
	$(MODULE_TARGET_VARIANT_TYPE) $(MODULE_LDFLAGS) -o $@ \
	$(MODULE_LIB_CRTBEGIN) $(MODULE_ALL_OBJECTS) $(MODULE_LIB_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) \
	$(MODULE_LIB_LDFLAGS)
endef

# Helper to convert a binary file into a C header. Can optionally
# null-terminate the binary before conversion.
#
# (1): Character array identifier
# (2): If non-empty, treat as a string and null terminate.
#      The character array will also be 'signed'.

define target-generate-header-from-binary
$(if $(V),,@echo "  OD      " $(call relative-to-top,$@))
$(ECHO) "static const $(if $(2),,unsigned )char $(1)[] = {" >$@
$(OD) $< -A n -t x1 -v | tr -d '\n' | \
	sed -r -e 's@^ @0x@' $(if $(2),-e 's@$$@ 00@',) -e 's@ @, 0x@g' \
	 -e 's@(([^[:blank:]]+[[:blank:]]+){8})@\1\n@g' >> $@
$(ECHO) "};" >> $@
endef

# Helper to convert an image file into a C header. The size of the
# image should be specified (but it is not checked).
#
# (1): Structure identifier
# (2): Width in pixels
# (3): Height in pixels

define target-generate-image-header-from-binary
$(if $(V),,@echo "  OD      " $(call relative-to-top,$@))
$(ECHO) "static const struct $(1)\
	{\n\tunsigned int width;\n\tunsigned int height;\n\tunsigned int byteLength;\n\tunsigned char data[$(shell stat -c %s $<)];\n}\
	$(1) = {\n\t$(2), $(3), sizeof($(1)), {" >$@
$(OD) $< -A n -t x1 -v | tr -d '\n' | \
	sed -r -e 's@^ @0x@' -e 's@ @, 0x@g' \
	 -e 's/(([^[:blank:]]+[[:blank:]]+){8})/\1\n/g' >>$@
$(ECHO) "}\n};" >>$@
endef

define host-shared-library-from-o
$(if $(V),,@echo "  HOST_LD " $(call relative-to-top,$@))
$(MODULE_CC) -shared -Wl,-Bsymbolic \
	$(MODULE_LDFLAGS) -o $@ \
	$(sort $(MODULE_ALL_OBJECTS)) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS)
endef

# If there were any C++ source files in a shared library, we use one of
# these recipes, which run the C++ compiler to link the final library
define target-shared-library-cxx-from-o
$(if $(V),,@echo "  LD      " $(call relative-to-top,$@))
$(MODULE_CXX) -shared -Wl,-Bsymbolic \
	$(MODULE_TARGET_VARIANT_TYPE) $(MODULE_LDFLAGS) -o $@ \
	$(MODULE_LIB_CRTBEGIN) $(MODULE_ALL_OBJECTS) $(MODULE_LIB_CRTEND) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS) \
	$(MODULE_LIB_LDFLAGS)
endef

define host-shared-library-cxx-from-o
$(if $(V),,@echo "  HOST_LD " $(call relative-to-top,$@))
$(MODULE_CXX) -shared -Wl,-Bsymbolic \
	$(MODULE_LDFLAGS) -o $@ \
	$(sort $(MODULE_ALL_OBJECTS)) \
	$(MODULE_LIBRARY_DIR_FLAGS) $(MODULE_LIBRARY_FLAGS)
endef

define host-copy-debug-information
$(MODULE_OBJCOPY) --only-keep-debug $@ $(basename $@).dbg
endef

define host-strip-debug-information
$(MODULE_STRIP) --strip-unneeded $@
endef

define host-add-debuglink
$(if $(V),,@echo "  DBGLINK " $(call relative-to-top,$(basename $@).dbg))
$(MODULE_OBJCOPY) --add-gnu-debuglink=$(basename $@).dbg $@
endef

define target-copy-debug-information
$(MODULE_OBJCOPY) --only-keep-debug $@ $(basename $@).dbg
endef

define target-strip-debug-information
$(MODULE_STRIP) --strip-unneeded $@
endef

define target-add-debuglink
$(if $(V),,@echo "  DBGLINK " $(call relative-to-top,$(basename $@).dbg))
$(MODULE_OBJCOPY) --add-gnu-debuglink=$(basename $@).dbg $@
endef

define target-compress-debug-information
$(MODULE_OBJCOPY) --compress-debug-sections $@ $@.compressed_debug
$(MV) $@.compressed_debug $@
endef

define host-static-library-from-o
$(if $(V),,@echo "  HOST_AR " $(call relative-to-top,$@))
$(RM) $@
$(MODULE_AR) crD $@ $(sort $(MODULE_ALL_OBJECTS))
endef

define target-static-library-from-o
$(if $(V),,@echo "  AR      " $(call relative-to-top,$@))
$(RM) $@
$(MODULE_AR) crD $@ $(sort $(MODULE_ALL_OBJECTS))
endef

define tab-c-from-y
$(if $(V),,@echo "  BISON   " $(call relative-to-top,$<))
$(BISON) $(MODULE_BISON_FLAGS) -o $@ -d $<
endef

define l-c-from-l
$(if $(V),,@echo "  FLEX    " $(call relative-to-top,$<))
$(FLEX) $(MODULE_FLEX_FLAGS) -o$@ $<
endef

define l-cc-from-l
$(if $(V),,@echo "  FLEXXX  " $(call relative-to-top,$<))
$(FLEXXX) $(MODULE_FLEXXX_FLAGS) -o$@ $<
endef

define clean-dirs
$(if $(V),,@echo "  RM      " $(call relative-to-top,$(MODULE_DIRS_TO_REMOVE)))
$(RM) -rf $(MODULE_DIRS_TO_REMOVE)
endef

define make-directory
$(MKDIR) -p $@
endef

ifeq ($(DISABLE_CHECK_EXPORTS),)
define check-exports
endef
else
define check-exports
endef
endif

# Check a source file with the program specified in $(CHECK).
# If $(CHECK) is empty, don't do anything.
ifeq ($(CHECK),)
check-src :=
else
# If CHECK is a relative path to something in the DDK then replace it with
# an absolute path. This is necessary for the kbuild target, which uses the
# Linux kernel build system, so that it can find the program specified in
# $(CHECK).
ifneq ($(wildcard $(TOP)/$(CHECK)),)
 override CHECK := $(TOP)/$(CHECK)
endif

define check-src-1
$(if $(V),,@echo "  CHECK   " $(call relative-to-top,$<))
$(if $(IGNORE_CHECK_ERRORS),-,)$(CHECK) $(MODULE_INCLUDE_FLAGS) \
	$(if $(CHECK_NO_CONFIG_H),,-include $(CONFIG_H)) \
	$(filter -D%,$(MODULE_CFLAGS)) \
	$(CHECKFLAGS) $<
endef
# If CHECK_ONLY is set, only check files matching a Make pattern.
# e.g. CHECK_ONLY=opengles1/%.c
define check-src
$(if $(and $(if $(CHECK_ONLY),$(filter $(CHECK_ONLY),$<),true), \
		$(if $(CHECK_EXCLUDE),$(filter-out $(CHECK_EXCLUDE),$<),true)),$(check-src-1),@:)
endef
endif

# Programs used in recipes

AR ?= ar
AR_SECONDARY ?= $(AR)
BISON ?= bison
CC ?= gcc
CC_SECONDARY ?= $(CC)
CROSS_COMPILE_SECONDARY ?= $(CROSS_COMPILE)
CXX ?= g++
CXX_SECONDARY ?= $(CXX)
GLSLC ?= glslc
HOST_AR ?= ar
HOST_AS ?= as
HOST_CC ?= gcc
HOST_CXX ?= g++
HOST_LD ?= ld
HOST_NM ?= nm
HOST_OBJCOPY ?= objcopy
HOST_OBJDUMP ?= objdump
HOST_RANLIB ?= ranlib
HOST_READELF ?= readelf
HOST_STRIP ?= strip
INDENT ?= indent
JAR ?= jar
JAVA ?= java
JAVAC ?= javac
M4 ?= m4
NM ?= nm
NM_SECONDARY ?= $(NM)
OBJCOPY ?= objcopy
OBJCOPY_SECONDARY ?= $(OBJCOPY)
PKG_CONFIG ?= pkg-config
PYTHON3 ?= python3
RANLIB ?= ranlib
RANLIB_SECONDARY ?= $(RANLIB)
STRIP ?= strip
STRIP_SECONDARY ?= $(STRIP)
ZIP ?= zip

ifneq ($(shell which python3),)
PYTHON ?= python3
else
PYTHON ?= python2

$(warning ******************************************************)
$(warning WARNING: Python 3 not found so falling back to Python)
$(warning 2, which is deprecated. See here for Python 2 end of)
$(warning life information:)
$(warning https://www.python.org/dev/peps/pep-0373/#id4)
$(warning ******************************************************)
endif

ifneq ($(SUPPORT_BUILD_LWS),)
WAYLAND_SCANNER := `$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner`
else
WAYLAND_SCANNER ?= wayland-scanner
endif

# Define CHMOD and CC_CHECK first so we can use cc-is-clang
#
override CHMOD		:= $(if $(V),,@)chmod
override CC_CHECK	:= $(if $(V),,@)$(MAKE_TOP)/tools/cc-check.sh

ifeq ($(USE_CCACHE),1)
 CCACHE ?= ccache
 ifeq ($(cc-is-clang),true)
  # Compiling with ccache and clang together can cause false errors
  # without this environment variable.
  export CCACHE_CPP2=1
 endif
endif
ifeq ($(USE_DISTCC),1)
 DISTCC ?= distcc
endif

# Toolchain triples for cross environments
#
CROSS_TRIPLE := $(patsubst %-,%,$(notdir $(CROSS_COMPILE)))
CROSS_TRIPLE_SECONDARY := $(patsubst %-,%,$(notdir $(CROSS_COMPILE_SECONDARY)))

# If clang is detected, the compiler name is invariant but CROSS_COMPILE
# is reflected in the use of -target. For GCC this is always encoded into
# the binary. If CROSS_COMPILE is not set we can skip this.
#
# If we're doing a build with multiple target architectures, we might need
# two separate compilers to build binaries for each architecture. In this
# case, CROSS_COMPILE and CROSS_COMPILE_SECONDARY are the cross compiler
# prefix for the two compilers - $(CC) and $(CC_SECONDARY).
#
# Set the secondary compiler first before we overwrite $(CC).
#

ifneq ($(CROSS_COMPILE_SECONDARY),)
 ifeq ($(cc-is-clang),true)
  __clang_target  := $(CROSS_TRIPLE_SECONDARY)
  ifeq ($(__clang_target),mips64el-linux-android)
   __clang_target := mipsel-linux-android
  endif
  __gcc_bindir  := $(dir $(shell which $(CROSS_COMPILE_SECONDARY)gcc))
  ifeq ($(wildcard $(__gcc_bindir)),)
   __gcc_bindir := $(dir $(CROSS_COMPILE_SECONDARY)gcc)
  endif
  override CC_SECONDARY   := \
   $(CC_SECONDARY) \
   -target $(__clang_target) \
   -B$(__gcc_bindir) \
   -B$(__gcc_bindir)/../$(CROSS_TRIPLE_SECONDARY)/bin \
   --gcc-toolchain=$(__gcc_bindir)/..
  override CXX_SECONDARY  := \
   $(CXX_SECONDARY) \
   -target $(__clang_target) \
   -B$(__gcc_bindir) \
   -B$(__gcc_bindir)/../$(CROSS_TRIPLE_SECONDARY)/bin \
   --gcc-toolchain=$(__gcc_bindir)/..
 else
  ifeq ($(origin CC_SECONDARY),file)
   override CC_SECONDARY  := $(CROSS_COMPILE_SECONDARY)$(CC_SECONDARY)
  endif
  ifeq ($(origin CXX_SECONDARY),file)
   override CXX_SECONDARY := $(CROSS_COMPILE_SECONDARY)$(CXX_SECONDARY)
  endif
 endif
 ifeq ($(origin AR_SECONDARY),file)
  override AR_SECONDARY  := $(CROSS_COMPILE_SECONDARY)$(AR_SECONDARY)
 endif
 ifeq ($(origin NM_SECONDARY),file)
  override NM_SECONDARY  := $(CROSS_COMPILE_SECONDARY)$(NM_SECONDARY)
 endif
 ifeq ($(origin OBJCOPY_SECONDARY),file)
  override OBJCOPY_SECONDARY  := $(CROSS_COMPILE_SECONDARY)$(OBJCOPY_SECONDARY)
 endif
 ifeq ($(origin RANLIB_SECONDARY),file)
  override RANLIB_SECONDARY  := $(CROSS_COMPILE_SECONDARY)$(RANLIB_SECONDARY)
 endif
 ifeq ($(origin STRIP_SECONDARY),file)
  override STRIP_SECONDARY  := $(CROSS_COMPILE_SECONDARY)$(STRIP_SECONDARY)
 endif
endif

# Vanilla versions of glibc >= 2.16 print a warning if _FORTIFY_SOURCE is
# defined but compiler optimisations are disabled. In this case, make sure it's
# not being defined as part of CC/CXX, as is the case for at least Yocto Poky
# 3.0.
ifeq ($(filter $(OPTIM),-O -O0),$(OPTIM))
 override CC_SECONDARY := $(filter-out -D_FORTIFY_SOURCE%,$(CC_SECONDARY))
 override CXX_SECONDARY := $(filter-out -D_FORTIFY_SOURCE%,$(CXX_SECONDARY))
else ifeq ($(BUILD),debug)
 override CC_SECONDARY := $(filter-out -D_FORTIFY_SOURCE%,$(CC_SECONDARY))
 override CXX_SECONDARY := $(filter-out -D_FORTIFY_SOURCE%,$(CXX_SECONDARY))
endif

# Apply compiler wrappers and V=1 handling
override AR_SECONDARY      := $(if $(V),,@)$(AR_SECONDARY)
override CC_SECONDARY      := $(if $(V),,@)$(strip $(CCACHE)$(DISTCC) $(CC_SECONDARY))
override CXX_SECONDARY     := $(if $(V),,@)$(strip $(CCACHE)$(DISTCC) $(CXX_SECONDARY))
override NM_SECONDARY      := $(if $(V),,@)$(NM_SECONDARY)
override OBJCOPY_SECONDARY := $(if $(V),,@)$(OBJCOPY_SECONDARY)
override RANLIB_SECONDARY  := $(if $(V),,@)$(RANLIB_SECONDARY)

ifneq ($(CROSS_COMPILE),)
 ifeq ($(cc-is-clang),true)
  __gcc_bindir  := $(dir $(shell which $(CROSS_COMPILE)gcc))
  ifeq ($(wildcard $(__gcc_bindir)),)
   __gcc_bindir := $(dir $(CROSS_COMPILE)gcc)
  endif
  override CC   := \
   $(CC) \
   -target $(CROSS_TRIPLE) \
   -B$(__gcc_bindir) \
   -B$(__gcc_bindir)/../$(CROSS_TRIPLE)/bin \
   --gcc-toolchain=$(__gcc_bindir)/..
  override CXX  := \
   $(CXX) \
   -target $(CROSS_TRIPLE) \
   -B$(__gcc_bindir) \
   -B$(__gcc_bindir)/../$(CROSS_TRIPLE)/bin \
   --gcc-toolchain=$(__gcc_bindir)/..
 else
  ifeq ($(origin CC),file)
   override CC  := $(CROSS_COMPILE)$(CC)
  endif
  ifeq ($(origin CXX),file)
   override CXX := $(CROSS_COMPILE)$(CXX)
  endif
 endif
 ifeq ($(origin AR),file)
  override AR  := $(CROSS_COMPILE)$(AR)
 endif
 ifeq ($(origin NM),file)
  override NM  := $(CROSS_COMPILE)$(NM)
 endif
 ifeq ($(origin OBJCOPY),file)
  override OBJCOPY  := $(CROSS_COMPILE)$(OBJCOPY)
 endif
 ifeq ($(origin RANLIB),file)
  override RANLIB  := $(CROSS_COMPILE)$(RANLIB)
 endif
 ifeq ($(origin STRIP),file)
  override STRIP  := $(CROSS_COMPILE)$(STRIP)
 endif
else
 $(if $(CROSS_COMPILE_SECONDARY),$(warning CROSS_COMPILE_SECONDARY is set but CROSS_COMPILE is empty))
endif

# Vanilla versions of glibc >= 2.16 print a warning if _FORTIFY_SOURCE is
# defined but compiler optimisations are disabled. In this case, make sure it's
# not being defined as part of CC/CXX, as is the case for at least Yocto Poky
# 3.0.
ifeq ($(filter $(OPTIM),-O -O0),$(OPTIM))
 override CC := $(filter-out -D_FORTIFY_SOURCE%,$(CC))
 override CXX := $(filter-out -D_FORTIFY_SOURCE%,$(CXX))
else ifeq ($(BUILD),debug)
 override CC := $(filter-out -D_FORTIFY_SOURCE%,$(CC))
 override CXX := $(filter-out -D_FORTIFY_SOURCE%,$(CXX))
endif

# Apply tool wrappers and V=1 handling.
#
# This list should be kept in alphabetical order.
#
override AR                := $(if $(V),,@)$(AR)
override BISON             := $(if $(V),,@)$(BISON)
override BZIP2             := $(if $(V),,@)bzip2 -9
override CAT               := $(if $(V),,@)cat
override CC                := $(if $(V),,@)$(strip $(CCACHE)$(DISTCC) $(CC))
override CHECK             := $(if $(CHECK),$(if $(V),,@)$(CHECK),)
override CP                := $(if $(V),,@)cp
override CXX               := $(if $(V),,@)$(strip $(CCACHE)$(DISTCC) $(CXX))
override ECHO              := $(if $(V),,@)$(shell which echo) -e
override FLEX              := $(if $(V),,@)flex
override FLEXXX            := $(if $(V),,@)flex++
override FWINFO            := $(if $(V),,@)$(HOST_OUT)/fwinfo
override GLSLC             := $(if $(V),,@)$(GLSLC)
override GREP              := $(if $(V),,@)grep
override HOST_AR           := $(if $(V),,@)$(HOST_AR)
override HOST_AS           := $(if $(V),,@)$(HOST_AS)
override HOST_CC           := $(if $(V),,@)$(strip $(CCACHE) $(HOST_CC))
override HOST_CXX          := $(if $(V),,@)$(strip $(CCACHE) $(HOST_CXX))
override HOST_LD           := $(if $(V),,@)$(HOST_LD)
override HOST_NM           := $(if $(V),,@)$(HOST_NM)
override HOST_OBJCOPY      := $(if $(V),,@)$(HOST_OBJCOPY)
override HOST_OBJDUMP      := $(if $(V),,@)$(HOST_OBJDUMP)
override HOST_RANLIB       := $(if $(V),,@)$(HOST_RANLIB)
override HOST_READELF      := $(if $(V),,@)$(HOST_READELF)
override HOST_STRIP        := $(if $(V),,@)$(HOST_STRIP)
override INSTALL           := $(if $(V),,@)install
override JAR               := $(if $(V),,@)$(JAR)
override JAVA              := $(if $(V),,@)$(JAVA)
override JAVAC             := $(if $(V),,@)$(JAVAC)
override LN                := $(if $(V),,@)ln -f -s
override M4                := $(if $(V),,@)$(M4)
override MKDIR             := $(if $(V),,@)mkdir
override MV                := $(if $(V),,@)mv
override NM                := $(if $(V),,@)$(NM)
override OBJCOPY           := $(if $(V),,@)$(OBJCOPY)
override OD                := $(if $(V),,@)od
override PERL              := $(if $(V),,@)perl
override PSC               := $(if $(V),,@)$(HOST_OUT)/psc_standalone
override PYTHON            := $(if $(V),,@)$(PYTHON)
override PYTHON3           := $(if $(V),,@)$(PYTHON3)
override RANLIB            := $(if $(V),,@)$(RANLIB)
override RM                := $(if $(V),,@)rm -f
override SED               := $(if $(V),,@)sed
override SIGNFILE          := $(if $(V),,@)$(KERNELDIR)/scripts/sign-file
override STRIP             := $(if $(V),,@)$(STRIP)
override STRIP_SECONDARY   := $(if $(V),,@)$(STRIP_SECONDARY)
override TAR               := $(if $(V),,@)tar
override TEST              := $(if $(V),,@)test
override TOUCH             := $(if $(V),,@)touch
override UNIFLEXC          := $(if $(V),,@)$(HOST_OUT)/usc
override USCASM            := $(if $(V),,@)$(HOST_OUT)/uscasm
override WAYLAND_SCANNER   := $(if $(V),,@)$(WAYLAND_SCANNER)
override ZIP               := $(if $(V),,@)$(ZIP)

ifeq ($(SUPPORT_NEUTRINO_PLATFORM),1)
include $(MAKE_TOP)/common/neutrino/commands_neutrino.mk
endif
