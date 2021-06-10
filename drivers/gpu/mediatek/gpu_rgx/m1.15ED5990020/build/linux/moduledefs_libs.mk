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

# This can't go in moduledefs_common.mk because it depends on
# MODULE_HOST_BUILD.

# MODULE_LIBRARY_FLAGS contains the flags to link each library. The rules
# are:
#
#  module_staticlibs := mylib
#  module_libs := mylib
#    Use -lmylib
#
#  module_extlibs := mylib
#    Use $(libmylib_ldflags) if that variable is defined (empty counts as
#    defined). Otherwise use -lmylib
#
#  module_libs := :mylib
#    Use -l:mylib.so
#
#  module_extlibs := :mylib.a
#    Use -l:mylib.a, but process *before* libgcc.a is linked. All other
#    extlibs are treated as dynamic and linked *after* libgcc.a.

MODULE_LIBRARY_FLAGS :=

# We want to do this only for pure Android, in which case only
# SUPPORT_ANDROID_PLATFORM will be set to 1.
ifeq ($(SUPPORT_ANDROID_PLATFORM)$(SUPPORT_ARC_PLATFORM),1)
 ifneq ($(filter $($(THIS_MODULE)_type),shared_library executable),)
  define set-extlibs-from-package
   ifeq ($(1),libdrm)
    $(THIS_MODULE)_extlibs += drm
   else ifeq ($(1),sync)
    $(THIS_MODULE)_extlibs += sync
   else
    $$(warning Unknown package for '$(THIS_MODULE)': $(1))
    $$(error Missing mapping between package and external library)
   endif
  endef

  $(foreach _package,$($(THIS_MODULE)_packages),\
   $(eval $(call set-extlibs-from-package,$(_package))))
 endif
endif

MODULE_LIBRARY_FLAGS += \
 $(addprefix -l, $($(THIS_MODULE)_staticlibs)) \
 $(addprefix -l, $(filter :%.a, $($(THIS_MODULE)_extlibs))) \
 $(if $(MODULE_HOST_BUILD),,$(MODULE_LIBGCC)) \
 $(addprefix -l, $(filter-out :%, $($(THIS_MODULE)_libs))) \
 $(addprefix -l, $(addsuffix .so, $(filter :%,$($(THIS_MODULE)_libs)))) \
 $(foreach _lib,$(filter-out :%.a, $($(THIS_MODULE)_extlibs)),$(if $(or $(MODULE_HOST_BUILD),$(filter undefined,$(origin lib$(_lib)_ldflags))),-l$(_lib),$(lib$(_lib)_ldflags)))

ifneq ($(MODULE_LIBRARY_FLAGS_SUBST),)
$(foreach _s,$(MODULE_LIBRARY_FLAGS_SUBST),$(eval \
 MODULE_LIBRARY_FLAGS := $(patsubst \
  -l$(word 1,$(subst :,$(space),$(_s))),\
  $(word 2,$(subst :,$(space),$(_s))),\
  $(MODULE_LIBRARY_FLAGS))))
endif

ifneq ($(SUPPORT_NEUTRINO_PLATFORM),1)
 # We don't want to do this for pure Android, in which case only
 # SUPPORT_ANDROID_PLATFORM will be set to 1.
 ifneq ($(SUPPORT_ANDROID_PLATFORM)$(SUPPORT_ARC_PLATFORM),1)
   $(foreach _package,$($(THIS_MODULE)_packages),\
    $(eval MODULE_LIBRARY_FLAGS     += `$(PKG_CONFIG) --libs-only-l $(_package)`))
 endif
endif

ifneq ($(SYSROOT),)
 ifneq ($(SYSROOT),/)
  ifeq (${MODULE_ARCH_TAG},armhf)
   MULTIARCH_DIR := arm-linux-gnueabihf
  else ifeq (${MODULE_ARCH_TAG},i686)
   MULTIARCH_DIR := i386-linux-gnu
  else
   MULTIARCH_DIR := ${MODULE_ARCH_TAG}-linux-gnu
  endif

  # Restrict pkg-config to looking only in the SYSROOT
  #
  # Sort paths based on priority. Local paths should always appear first to
  # ensure that user built packages override the system versions. Driver paths
  # should appear last to ensure shim libraries (if present) get priority.
  PKG_CONFIG_LIBDIR := ${SYSROOT}/usr/local/lib/${MULTIARCH_DIR}/pkgconfig
  PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/local/lib/pkgconfig
  PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/local/share/pkgconfig
  PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/lib/${MULTIARCH_DIR}/pkgconfig
  PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/lib64/pkgconfig
  PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/lib/pkgconfig
  PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/share/pkgconfig
  PKG_CONFIF_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/lib64/driver/pkgconfig
  PKG_CONFIG_LIBDIR := $(PKG_CONFIG_LIBDIR):${SYSROOT}/usr/lib/driver/pkgconfig

  # SYSROOT doesn't always do the right thing. So explicitly add necessary
  # paths to the link path
  MODULE_LDFLAGS += -Xlinker -rpath-link=${SYSROOT}/usr/local/lib/${MULTIARCH_DIR}
  MODULE_LDFLAGS += -Xlinker -rpath-link=${SYSROOT}/lib/${MULTIARCH_DIR}
  MODULE_LDFLAGS += -Xlinker -rpath-link=${SYSROOT}/usr/lib/
  MODULE_LDFLAGS += -Xlinker -rpath-link=${SYSROOT}/usr/lib/${MULTIARCH_DIR}
 endif
endif

ifneq ($(MODULE_ARCH_TAG),)
 MODULE_LIBRARY_DIR_FLAGS := $(subst _LLVM_ARCH_,$(MODULE_ARCH_TAG),$(MODULE_LIBRARY_DIR_FLAGS))
 MODULE_INCLUDE_FLAGS     := $(subst _LLVM_ARCH_,$(MODULE_ARCH_TAG),$(MODULE_INCLUDE_FLAGS))

 MODULE_LIBRARY_DIR_FLAGS := $(subst _NNVM_ARCH_,$(MODULE_ARCH_TAG),$(MODULE_LIBRARY_DIR_FLAGS))
 MODULE_INCLUDE_FLAGS     := $(subst _NNVM_ARCH_,$(MODULE_ARCH_TAG),$(MODULE_INCLUDE_FLAGS))
endif
