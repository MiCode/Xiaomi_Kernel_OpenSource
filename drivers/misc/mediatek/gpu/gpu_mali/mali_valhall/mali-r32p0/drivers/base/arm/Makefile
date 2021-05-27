# SPDX-License-Identifier: GPL-2.0
#
# (C) COPYRIGHT 2021 ARM Limited. All rights reserved.
#
# This program is free software and is provided to you under the terms of the
# GNU General Public License version 2 as published by the Free Software
# Foundation, and any use by you of this program is subject to the terms
# of such GNU license.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, you can access it online at
# http://www.gnu.org/licenses/gpl-2.0.html.
#
#

#
# Paths
#
KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
KDIR ?= $(KERNEL_SRC)

ifeq ($(KDIR),)
    $(error Must specify KDIR to point to the kernel to target))
endif

vars :=
#
# Default configuration values
#
CONFIG_MALI_BASE_MODULES ?= n

ifeq ($(CONFIG_MALI_BASE_MODULES),y)
    CONFIG_MALI_CSF_SUPPORT ?= n

    ifneq ($(CONFIG_DMA_SHARED_BUFFER),n)
        CONFIG_DMA_SHARED_BUFFER_TEST_EXPORTER ?= m
    else
        # Prevent misuse when CONFIG_DMA_SHARED_BUFFER=n
        CONFIG_DMA_SHARED_BUFFER_TEST_EXPORTER = n
    endif

    CONFIG_MALI_MEMORY_GROUP_MANAGER ?= m

    ifneq ($(CONFIG_MALI_CSF_SUPPORT), n)
        CONFIG_MALI_PROTECTED_MEMORY_ALLOCATOR ?= m
    endif

else
    # Prevent misuse when CONFIG_MALI_BASE_MODULES=n
    CONFIG_DMA_SHARED_BUFFER_TEST_EXPORTER = n
    CONFIG_MALI_MEMORY_GROUP_MANAGER = n
    CONFIG_MALI_PROTECTED_MEMORY_ALLOCATOR = n

endif

CONFIGS := \
    CONFIG_MALI_BASE_MODULES \
    CONFIG_MALI_CSF_SUPPORT \
    CONFIG_DMA_SHARED_BUFFER_TEST_EXPORTER \
    CONFIG_MALI_MEMORY_GROUP_MANAGER \
    CONFIG_MALI_PROTECTED_MEMORY_ALLOCATOR


# Find which config options were enabled. $(value config) is the name of the
# config option (CONFIG_MALI_...), $(value $(value config)) is its value (y, m,
# or n).
ENABLED_CONFIGS := $(foreach config,$(CONFIGS),$(if $(filter y m,$(value $(value config))),$(config)))

# For each enabled option, generate a command line argument e.g.
# CONFIG_MALI_BASE_MODULES=y.
MAKE_ARGS := $(foreach config,$(ENABLED_CONFIGS),$(value config)=$(value $(value config)))
# For each enabled option, generate a compiler '-DCONFIG_...=$(CONFIG_...)' flag.
EXTRA_CFLAGS := $(foreach config,$(ENABLED_CONFIGS),-D$(value config)=$(value config))

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(MAKE_ARGS) EXTRA_CFLAGS="$(EXTRA_CFLAGS)" KBUILD_EXTRA_SYMBOLS="$(EXTRA_SYMBOLS)" modules

modules_install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(MAKE_ARGS) modules_install

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(MAKE_ARGS) clean
