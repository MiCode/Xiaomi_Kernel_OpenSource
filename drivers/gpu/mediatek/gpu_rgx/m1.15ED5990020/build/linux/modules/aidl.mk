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

ifneq ($(wildcard $(OUT_DIR)/host/$(HOST_OS)-$(HOST_PREBUILT_ARCH)/bin/aidl),)
override AIDL_BIN := $(OUT_DIR)/host/$(HOST_OS)-$(HOST_PREBUILT_ARCH)/bin/aidl
else
override AIDL_BIN := $(OUT_DIR)/soong/host/$(HOST_OS)-$(HOST_PREBUILT_ARCH)/bin/aidl
endif

define gen_hdrs_targets
$(addprefix $(GENERATED_CODE_OUT)/$(1)/$($(1)_type)/$($(1)_intf_path)/,\
 $(foreach _i,$($(1)_intf_class),Bn$(_i)$(2)h Bp$(_i)$(2)h $(_i)$(2)h))
endef

# It could be ndk|cpp|java, and the default generated headers are for ndk.
ifneq ($($(THIS_MODULE)_lang),)
 # Only support to generate headers for ndk and cpp.
 ifneq ($(filter ndk cpp,$($(THIS_MODULE)_lang)),)
  MODULE_LANGUAGE := $($(THIS_MODULE)_lang)
 else
  $(error Invalid language type for AIDL header generation.)
 endif
else
 MODULE_LANGUAGE := ndk
endif

# Default is $ANDROID_ROOT
ifneq ($($(THIS_MODULE)_base_path),)
 MODULE_BASE_PATH := $($(THIS_MODULE)_base_path)
else
 MODULE_BASE_PATH := $(ANDROID_ROOT)
endif
MODULE_TARGET_DIR := $(call addprefix-ifnot-abs,$(TOP)/,$(GENERATED_CODE_OUT)/$(THIS_MODULE))
MODULE_SRCS := $(addprefix $(MODULE_BASE_PATH)/,$($(THIS_MODULE)_src))

# The paths of import modules
MODULE_IMPORT_PATHS := $(addprefix -I,$($(THIS_MODULE)_import_paths))

# Rule for generating the AIDL headers
MODULE_HDRS_TARGETS := $(call gen_hdrs_targets,$(THIS_MODULE),.)
MODULE_TARGETS := $(MODULE_HDRS_TARGETS)
# Make doesn't handle rules that generate multiple files. Use a pattern rule
# and define name%h. so only the dot is used as the common part.
MODULE_HDRS_TARGETS_PATTERN := $(call gen_hdrs_targets,$(THIS_MODULE),%)

$(MODULE_HDRS_TARGETS_PATTERN): MODULE_BASE_PATH := $(MODULE_BASE_PATH)
$(MODULE_HDRS_TARGETS_PATTERN): MODULE_PACKAGE_PATH := $($(THIS_MODULE)_package_path)
$(MODULE_HDRS_TARGETS_PATTERN): MODULE_OUT := $(MODULE_TARGET_DIR)
$(MODULE_HDRS_TARGETS_PATTERN): MODULE_HDRS_TARGETS := $(MODULE_HDRS_TARGETS)
$(MODULE_HDRS_TARGETS_PATTERN): MODULE_OPTIONAL_FLAGS := $($(THIS_MODULE)_optional_flags)
$(MODULE_HDRS_TARGETS_PATTERN): MODULE_LANGUAGE := $(MODULE_LANGUAGE)
$(MODULE_HDRS_TARGETS_PATTERN): MODULE_SRCS := $(MODULE_SRCS)
$(MODULE_HDRS_TARGETS_PATTERN): MODULE_IMPORT_PATHS := $(MODULE_IMPORT_PATHS)
$(MODULE_HDRS_TARGETS_PATTERN): $(THIS_MAKEFILE) | $(MODULE_OUT)
	$(if $(V),,@echo "  AIDL-HDR" $(call relative-to-top,$(notdir $(MODULE_HDRS_TARGETS))))
	$(if $(V),,@)(cd $(MODULE_BASE_PATH) && $(AIDL_BIN) --lang=$(MODULE_LANGUAGE) $(MODULE_OPTIONAL_FLAGS) --structured -h $(MODULE_OUT) -o $(MODULE_OUT) $(MODULE_IMPORT_PATHS) $(MODULE_SRCS))

.PHONY: $(THIS_MODULE)
$(THIS_MODULE): $(MODULE_TARGETS)
