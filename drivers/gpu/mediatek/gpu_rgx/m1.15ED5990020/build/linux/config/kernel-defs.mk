############################################################################ ###
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
### ############################################################################

# The purpose of this file is to keep KernelConfig macros isolated from the
# BothConfig and UserConfig macros. This allows this file to be removed from
# the user mode package and, thus, avoids the kernel config files being
# generated when building from this package.
#

# Write out a kernel GNU make option.
#
define KernelConfigMake
$$(shell echo "override $(1) $(if $(2),:= $(strip $(2)),:=)" >>$(CONFIG_KERNEL_MK).new)
$(if $(filter config,$(D)),$(info KernelConfigMake $(1) := $(2)	# $(if $($(1)),$(origin $(1)),default)))
endef

# Conditionally write out a kernel GNU make option
#
define _TunableKernelConfigMake
ifneq ($$($(1)),)
ifneq ($$($(1)),0)
$$(eval $$(call KernelConfigMake,$(1),$$($(1))))
else
unexport $(1)
endif
else
ifneq ($(2),)
$$(eval $$(call KernelConfigMake,$(1),$(2)))
else
unexport $(1)
endif
endif
endef

define TunableKernelConfigMake
$$(eval $$(call _TunableKernelConfigMake,$(1),$(2)))
$(call RegisterOptionHelp,$(1),$(2),$(3),$(4))
endef

# Write out a kernel-only option
#
define KernelConfigC
$$(shell echo "#define $(if $(2),$(1) $(2),$(1))" >>$(CONFIG_KERNEL_H).new)
$(if $(filter config,$(D)),$(info KernelConfigC    #define $(1) $(2)	/* $(if $($(1)),$(origin $(1)),default) */),)
endef

# Write out kernel-only AppHint defaults as specified
#
define AppHintConfigC
ifneq ($$($(1)),)
$$(eval $$(call KernelConfigC,$(1),$$($(1))))
else
$$(eval $$(call KernelConfigC,$(1),$(2)))
endif
$(call RegisterOptionHelp,$(1),$(2),$(3),$(4))
endef

# Write out kernel-only AppHint
# Converts user supplied value to a set of ORed flags or defaults as specified
#
define AppHintFlagsConfigC
ifneq ($$($(1)),)
$$(eval $$(call ValidateValues,$(1),$(4)))
_$(1)_FLAGS := $$(subst $$(comma),$$(space),$$($(1)))
_$(1)_FLAGS := $$(strip $$(foreach group,$$(_$(1)_FLAGS),$(3)$$(group)))
_$(1)_FLAGS := $$(subst $$(space), | ,$$(_$(1)_FLAGS))
$$(eval $$(call KernelConfigC,$(1),$$(_$(1)_FLAGS)))
else
$$(eval $$(call KernelConfigC,$(1),$(2)))
endif
$(call RegisterOptionHelp,$(1),$(2),$(5))
endef

# Conditionally write out a kernel-only option
#
define _TunableKernelConfigC
ifneq ($$($(1)),)
ifneq ($$($(1)),0)
ifeq ($$($(1)),1)
$$(eval $$(call KernelConfigC,$(1),))
else
$$(eval $$(call KernelConfigC,$(1),$$($(1))))
endif
endif
else
ifneq ($(2),)
ifeq ($(2),1)
$$(eval $$(call KernelConfigC,$(1),))
else
$$(eval $$(call KernelConfigC,$(1),$(2)))
endif
endif
endif
endef

define TunableKernelConfigC
$$(eval $$(call _TunableKernelConfigC,$(1),$(2)))
$(call RegisterOptionHelp,$(1),$(2),$(3),$(4))
endef

# delete any previous intermediary files
$(shell \
	for file in $(CONFIG_KERNEL_H).new $(CONFIG_KERNEL_MK).new ; do \
		rm -f $$file; \
	done)
