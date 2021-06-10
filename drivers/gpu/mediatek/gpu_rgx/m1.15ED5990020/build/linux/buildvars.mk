########################################################################### ###
#@Title         Define global variables
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@Description   This file is read once at the start of the build, after reading
#               in config.mk. It should define the non-MODULE_* variables used
#               in commands, like ALL_CFLAGS
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

# NOTE: You must *not* use the cc-option et al macros in COMMON_FLAGS,
# COMMON_CFLAGS or COMMON_USER_FLAGS. These flags are shared between
# host and target, which might use compilers with different capabilities.

# ANOTHER NOTE: All flags here must be architecture-independent (i.e. no
# -march or toolchain include paths)

# These flags are used for kernel, User C and User C++
#
COMMON_FLAGS := -W -Wall

# Enable 64-bit file & memory handling on 32-bit systems.
#
# This only affects glibc and possibly other Linux libc implementations; it
# does not apply to Android where _FILE_OFFSET_BITS is not completely
# implemented in bionic, and _LARGEFILE{,64}_SOURCE do not apply.
#
# This makes no difference on 64-bit systems, and allows for file and
# memory addresses >2GB to be handled on 32-bit systems.
#
#   _LARGEFILE_SOURCE     adds a couple functions (fseeko & ftello)
#   _LARGEFILE64_SOURCE   adds *64 variants of 32-bit file operations
#   _FILE_OFFSET_BITS=64  makes the 64-bit variants the default
#
ifneq ($(SUPPORT_ANDROID_PLATFORM),1)
COMMON_FLAGS += \
 -D_LARGEFILE_SOURCE \
 -D_LARGEFILE64_SOURCE \
 -D_FILE_OFFSET_BITS=64
endif

ifeq (, $(shell which indent))
 INDENT_TOOL_NOT_FOUND := 1
else
 INDENT_GENERATED_HEADERS := 1
endif

# Some GCC warnings are C only, so we must mask them from C++
#
COMMON_CFLAGS := $(COMMON_FLAGS) \
 -Wno-format-zero-length \
 -Wmissing-prototypes -Wstrict-prototypes

# User C and User C++ optimization control. Does not affect kernel.
#
ifeq ($(BUILD),debug)
COMMON_USER_FLAGS := -O0
else
OPTIM ?= -O2
ifneq ($(PVRSRV_NEED_PVR_ASSERT),1)
COMMON_USER_FLAGS := -DNDEBUG
endif
ifeq ($(USE_LTO),1)
COMMON_USER_FLAGS += $(OPTIM) -flto
else
COMMON_USER_FLAGS += $(OPTIM)
endif
endif

# GCOV support for user-mode coverage statistics
#
ifeq ($(GCOV_BUILD),on)
COMMON_USER_FLAGS += -fprofile-arcs -ftest-coverage
endif

# Driver has not yet been audited for aliasing issues
#
COMMON_USER_FLAGS += -fno-strict-aliasing

# We always enable debugging. Either the release binaries are stripped
# and the symbols put in the symbolpackage, or we're building debug.
#
COMMON_USER_FLAGS += -g

# User C and User C++ warning flags
#
COMMON_USER_FLAGS += \
 -Wpointer-arith -Wunused-parameter \
 -Wmissing-format-attribute

# Additional warnings, and optional warnings.
#
TESTED_TARGET_USER_FLAGS := \
 $(call cc-option,-Wno-error=implicit-fallthrough) \
 $(call cc-option,-Wno-missing-field-initializers) \
 $(call cc-option,-Wno-error=assume) \
 $(call cc-option,-fdiagnostics-show-option) \
 $(call cc-option,-Wno-self-assign) \
 $(call cc-option,-Wno-parentheses-equality)
TESTED_HOST_USER_FLAGS := \
 $(call host-cc-option,-Wno-error=implicit-fallthrough) \
 $(call host-cc-option,-Wno-missing-field-initializers) \
 $(call host-cc-option,-fdiagnostics-show-option) \
 $(call host-cc-option,-Wno-self-assign) \
 $(call host-cc-option,-Wno-parentheses-equality)

# These flags are clang-specific.
# -Wno-unused-command-line-argument works around a buggy interaction
# with ccache, see https://bugzilla.samba.org/show_bug.cgi?id=8118
# -fcolor-diagnostics force-enables colored error messages which
# get disabled when ccache is piped through ccache.
#
TESTED_TARGET_USER_FLAGS += \
 $(call cc-option,-Qunused-arguments) \
 $(call cc-option,-Wlogical-op) \
 $(if $(shell test -t 2 && echo true),$(call cc-option,-fcolor-diagnostics))
TESTED_HOST_USER_FLAGS += \
 $(call host-cc-option,-Qunused-arguments) \
 $(call host-cc-option,-Wlogical-op) \
 $(if $(shell test -t 2 && echo true),$(call host-cc-option,-fcolor-diagnostics))

ifeq ($(W),1)
TESTED_TARGET_USER_FLAGS += \
 $(call cc-option,-Wbad-function-cast) \
 $(call cc-option,-Wcast-qual) \
 $(call cc-option,-Wcast-align) \
 $(call cc-option,-Wconversion) \
 $(call cc-option,-Wdisabled-optimization) \
 $(call cc-option,-Wmissing-declarations) \
 $(call cc-option,-Wmissing-include-dirs) \
 $(call cc-option,-Wnested-externs) \
 $(call cc-option,-Wold-style-definition) \
 $(call cc-option,-Woverlength-strings) \
 $(call cc-option,-Wpacked) \
 $(call cc-option,-Wpacked-bitfield-compat) \
 $(call cc-option,-Wpadded) \
 $(call cc-option,-Wredundant-decls) \
 $(call cc-option,-Wshadow) \
 $(call cc-option,-Wswitch-default) \
 $(call cc-option,-Wvla) \
 $(call cc-option,-Wwrite-strings)
TESTED_HOST_USER_FLAGS += \
 $(call host-cc-option,-Wbad-function-cast) \
 $(call host-cc-option,-Wcast-qual) \
 $(call host-cc-option,-Wcast-align) \
 $(call host-cc-option,-Wconversion) \
 $(call host-cc-option,-Wdisabled-optimization) \
 $(call host-cc-option,-Wmissing-declarations) \
 $(call host-cc-option,-Wmissing-include-dirs) \
 $(call host-cc-option,-Wnested-externs) \
 $(call host-cc-option,-Wold-style-definition) \
 $(call host-cc-option,-Woverlength-strings) \
 $(call host-cc-option,-Wpacked) \
 $(call host-cc-option,-Wpacked-bitfield-compat) \
 $(call host-cc-option,-Wpadded) \
 $(call host-cc-option,-Wredundant-decls) \
 $(call host-cc-option,-Wshadow) \
 $(call host-cc-option,-Wswitch-default) \
 $(call host-cc-option,-Wvla) \
 $(call host-cc-option,-Wwrite-strings)
endif

TESTED_TARGET_USER_FLAGS += \
 $(call cc-optional-warning,-Wunused-but-set-variable) \
 $(call cc-optional-warning,-Wtypedef-redefinition)
TESTED_HOST_USER_FLAGS += \
 $(call host-cc-optional-warning,-Wunused-but-set-variable) \
 $(call host-cc-optional-warning,-Wtypedef-redefinition)

KBUILD_FLAGS := \
 -Wno-unused-parameter -Wno-sign-compare

# Add '-g' for the case where CONFIG_DEBUG_INFO isn't enabled in the kernel config.
# Although this means the kernel won't contain symbols, the driver module will.
# This is useful for matching up stack traces to source lines in the driver code.
# To get full symbols (kernel and driver), the kernel will need to be configured
# and built with CONFIG_DEBUG_INFO enabled.
ifeq ($(BUILD),debug)
KBUILD_FLAGS += -g
endif

TESTED_KBUILD_FLAGS := \
 $(call kernel-cc-option,-Wmissing-include-dirs) \
 $(call kernel-cc-option,-Wno-type-limits) \
 $(call kernel-cc-option,-Wno-pointer-arith) \
 $(call kernel-cc-option,-Wno-pointer-sign) \
 $(call kernel-cc-option,-Wno-aggregate-return) \
 $(call kernel-cc-option,-Wno-unused-but-set-variable) \
 $(call kernel-cc-option,-Wno-ignored-qualifiers) \
 $(call kernel-cc-option,-Wno-error=implicit-fallthrough) \
 $(call kernel-cc-optional-warning,-Wbad-function-cast) \
 $(call kernel-cc-optional-warning,-Wcast-qual) \
 $(call kernel-cc-optional-warning,-Wcast-align) \
 $(call kernel-cc-optional-warning,-Wconversion) \
 $(call kernel-cc-optional-warning,-Wdisabled-optimization) \
 $(call kernel-cc-optional-warning,-Wlogical-op) \
 $(call kernel-cc-optional-warning,-Wmissing-declarations) \
 $(call kernel-cc-optional-warning,-Wmissing-include-dirs) \
 $(call kernel-cc-optional-warning,-Wnested-externs) \
 $(call kernel-cc-optional-warning,-Wno-missing-field-initializers) \
 $(call kernel-cc-optional-warning,-Wold-style-definition) \
 $(call kernel-cc-optional-warning,-Woverlength-strings) \
 $(call kernel-cc-optional-warning,-Wpacked) \
 $(call kernel-cc-optional-warning,-Wpacked-bitfield-compat) \
 $(call kernel-cc-optional-warning,-Wpadded) \
 $(call kernel-cc-optional-warning,-Wredundant-decls) \
 $(call kernel-cc-optional-warning,-Wshadow) \
 $(call kernel-cc-optional-warning,-Wswitch-default) \
 $(call kernel-cc-optional-warning,-Wwrite-strings)

# Force no-pie, for compilers that enable pie by default
TESTED_KBUILD_FLAGS := \
 $(call kernel-cc-option,-fno-pie) \
 $(call kernel-cc-option,-no-pie) \
 $(TESTED_KBUILD_FLAGS)

# When building against experimentally patched kernels with LLVM support,
# we need to suppress warnings about bugs we haven't fixed yet. This is
# temporary and will go away in the future.
ifeq ($(kernel-cc-is-clang),true)
TESTED_KBUILD_FLAGS := \
 $(call kernel-cc-option,-Wno-address-of-packed-member) \
 $(call kernel-cc-option,-Wno-unneeded-internal-declaration) \
 $(call kernel-cc-option,-Wno-unused-function) \
 $(call kernel-cc-optional-warning,-Wno-typedef-redefinition) \
 $(call kernel-cc-optional-warning,-Wno-sometimes-uninitialized) \
 $(TESTED_KBUILD_FLAGS)
endif

# User C only
#
ALL_CFLAGS := \
 -std=gnu99 \
 $(COMMON_USER_FLAGS) $(COMMON_CFLAGS) $(TESTED_TARGET_USER_FLAGS) \
 $(SYS_CFLAGS)
ALL_HOST_CFLAGS := \
 -std=gnu99 \
 $(COMMON_USER_FLAGS) $(COMMON_CFLAGS) $(TESTED_HOST_USER_FLAGS)

# User C++ only
#
ALL_CXXFLAGS := \
 -std=gnu++11 \
 -fno-rtti -fno-exceptions \
 $(COMMON_USER_FLAGS) $(COMMON_FLAGS) $(TESTED_TARGET_USER_FLAGS) \
 $(SYS_CXXFLAGS)
ALL_HOST_CXXFLAGS := \
 -std=gnu++11 \
 -fno-rtti -fno-exceptions \
 $(COMMON_USER_FLAGS) $(COMMON_FLAGS) $(TESTED_HOST_USER_FLAGS)

ifeq ($(PERFDATA),1)
ALL_CFLAGS += -funwind-tables
endif

# Workaround for clang is producing wrong code when -O0 is used.
# Applies only for clang < 3.8
#
ifeq ($(cc-is-clang),true)
__clang_bindir  := $(dir $(shell which clang))
__clang_version := $(shell clang --version | grep -P -o '(?<=clang version )([0-9][^ ]+)')
__clang_major := $(shell echo $(__clang_version) | cut -f1 -d'.')
__clang_minor := $(shell echo $(__clang_version) | cut -f2 -d'.')
ifneq ($(filter -O0,$(ALL_CFLAGS)),)
__clang_lt_3.8 := \
	$(shell ((test $(__clang_major) -lt 3) || \
	        ((test $(__clang_major) -eq 3) && (test $(__clang_minor) -lt 8))) && echo 1 || echo 0)
ifeq ($(__clang_lt_3.8),1)
ALL_CFLAGS := $(patsubst -O0,-O1,$(ALL_CFLAGS))
ALL_CXXFLAGS := $(patsubst -O0,-O1,$(ALL_CXXFLAGS))
endif
endif
endif

# Add GCOV_DIR just for target
#
ifeq ($(GCOV_BUILD),on)
ifneq ($(GCOV_DIR),)
ALL_CFLAGS += -fprofile-dir=$(GCOV_DIR)
ALL_CXXFLAGS += -fprofile-dir=$(GCOV_DIR)
endif
endif

# Kernel C only
#
ALL_KBUILD_CFLAGS := $(COMMON_CFLAGS) $(KBUILD_FLAGS) $(TESTED_KBUILD_FLAGS)

# User C and C++
#
# NOTE: ALL_HOST_LDFLAGS should probably be using -rpath-link too, and if we
# ever need to support building host shared libraries, it's required.
#
# We can't use it right now because we want to support non-GNU-compatible
# linkers like the Darwin 'ld' which doesn't support -rpath-link.
#
# For the same reason (Darwin 'ld') don't bother checking for text
# relocations in host binaries.
#
ALL_HOST_LDFLAGS :=
ALL_LDFLAGS := -Wl,--warn-shared-textrel

ifneq ($(USE_GOLD_LINKER),)
ALL_HOST_LDFLAGS += -fuse-ld=gold
ALL_LDFLAGS +=-fuse-ld=gold
endif

ifeq ($(GCOV_BUILD),on)
ALL_LDFLAGS += -fprofile-arcs
ALL_HOST_LDFLAGS += -fprofile-arcs
endif

ALL_LDFLAGS += $(SYS_LDFLAGS)

# Optional security hardening features.
# Roughly matches Android's default security build options.
ifneq ($(FORTIFY),)
 ALL_CFLAGS   += -fstack-protector -Wa,--noexecstack
 ALL_CXXFLAGS += -fstack-protector -Wa,--noexecstack
 ALL_LDFLAGS  += -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now

 # Vanilla versions of glibc >= 2.16 print a warning if _FORTIFY_SOURCE is
 # defined but compiler optimisations are disabled.
 ifneq ($(BUILD),debug)
  ifneq ($(filter-out -O -O0,$(OPTIM)),)
   ALL_CFLAGS   += -D_FORTIFY_SOURCE=2
   ALL_CXXFLAGS += -D_FORTIFY_SOURCE=2
  endif
 endif
endif

# Sanitiser support
ifneq ($(USE_SANITISER),)
 ifeq ($(USE_SANITISER),1)
  # Default sanitisers
  override USE_SANITISER := address,undefined
 endif
 $(info Including the following sanitisers: $(USE_SANITISER))
 ALL_CFLAGS   += -fsanitize=$(USE_SANITISER)
 ALL_CXXFLAGS += -fsanitize=$(USE_SANITISER)
 ALL_LDFLAGS  += -fsanitize=$(USE_SANITISER)
 ALL_HOST_CFLAGS   += -fsanitize=$(USE_SANITISER)
 ALL_HOST_CXXFLAGS += -fsanitize=$(USE_SANITISER)
 ALL_HOST_LDFLAGS  += -fsanitize=$(USE_SANITISER)
 ifeq ($(cc-is-clang),false)
  ALL_HOST_LDFLAGS  += -static-libasan
 endif
endif

# This variable contains a list of all modules built by kbuild
ALL_KBUILD_MODULES :=

# This variable contains a list of all modules which contain C++ source files
ALL_CXX_MODULES :=

ifneq ($(TOOLCHAIN),)
$(warning **********************************************)
$(warning  The TOOLCHAIN option has been removed, but)
$(warning  you have it set (via $(origin TOOLCHAIN)))
$(warning **********************************************)
endif

# We need the glibc version to generate the cache names for LLVM and XOrg components.
ifeq ($(CROSS_COMPILE),)
LIBC_VERSION_PROBE := $(shell ldd  $(shell which true) | awk '/libc.so/{print $$3'} )
LIBC_VERSION := $(shell $(LIBC_VERSION_PROBE)| tr -d '(),' | head -1)
endif
