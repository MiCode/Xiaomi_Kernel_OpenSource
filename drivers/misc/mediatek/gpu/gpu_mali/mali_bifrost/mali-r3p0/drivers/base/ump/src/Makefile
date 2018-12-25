#
# (C) COPYRIGHT 2008-2014 ARM Limited. All rights reserved.
#
# This program is free software and is provided to you under the terms of the
# GNU General Public License version 2 as published by the Free Software
# Foundation, and any use by you of this program is subject to the terms
# of such GNU licence.
#
# A copy of the licence is included with the program, and can also be obtained
# from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA.
#
#



ifneq ($(KBUILD_EXTMOD),)
include $(KBUILD_EXTMOD)/Makefile.common
else
include ./Makefile.common
endif

# default to building for the host
ARCH ?= $(shell uname -m)

# linux build system integration
RELATIVE_ROOT=../../../../..
ROOT = $(CURDIR)/$(RELATIVE_ROOT)

EXTRA_CFLAGS=-I$(CURDIR)/../../../../include

ifeq ($(MALI_UNIT_TEST),1)
	EXTRA_CFLAGS += -DMALI_UNIT_TEST=$(MALI_UNIT_TEST)
endif

# Get any user defined KDIR-<names> or maybe even a hardcoded KDIR
-include KDIR_CONFIGURATION

# Define host system directory
KDIR-$(shell uname -m):=/lib/modules/$(shell uname -r)/build

CONFIG ?= $(ARCH)

# default cpu to select
CPU ?= $(shell uname -m)

# look up KDIR based om CPU selection
KDIR ?= $(KDIR-$(CPU))

ifeq ($(KDIR),)
$(error No KDIR found for platform $(CPU))
endif

# Validate selected config
ifneq ($(shell [ -d arch-$(CONFIG) ] && [ -f arch-$(CONFIG)/config.h ] && echo "OK"), OK)
$(warning Current directory is $(shell pwd))
$(error No configuration found for config $(CONFIG). Check that arch-$(CONFIG)/config.h exists)
else
# Link arch to the selected arch-config directory
$(shell [ -L arch ] && rm arch)
$(shell ln -sf arch-$(CONFIG) arch)
$(shell touch arch/config.h)
endif

EXTRA_SYMBOLS=

ifeq ($(MALI_UNIT_TEST),1)
	KBASE_PATH=$(ROOT)/kernel/drivers/gpu/arm/midgard
	EXTRA_SYMBOLS+=$(KBASE_PATH)/tests/internal/src/kernel_assert_module/linux/Module.symvers
endif
KDS_PATH=$(ROOT)/kernel/drivers/base/kds
EXTRA_SYMBOLS+=$(KDS_PATH)/Module.symvers

all:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) EXTRA_CFLAGS="$(EXTRA_CFLAGS) $(SCONS_CFLAGS)" CONFIG_UMP=m KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" modules

kernelrelease:
	$(MAKE) -C $(KDIR) KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" kernelrelease

clean:
	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) clean
