#
# This confidential and proprietary software may be used only as
# authorised by a licensing agreement from ARM Limited
# (C) COPYRIGHT 2008-2012, 2014-2015 ARM Limited
# ALL RIGHTS RESERVED
# The entire notice above must be reproduced on all authorised
# copies and copies may only be made to the extent permitted
# by a licensing agreement from ARM Limited.
#

# For each arch check: CROSS_COMPILE , KDIR , CFLAGS += -DARCH

export ARCH ?= arm
BUILD ?= debug

check_cc2 = \
	$(shell if $(1) -S -o /dev/null -xc /dev/null > /dev/null 2>&1; \
	then \
		echo "$(2)"; \
	else \
		echo "$(3)"; \
	fi ;)

# Check that required parameters are supplied.
ifeq ($(CONFIG),)
CONFIG := default
endif
ifeq ($(CPU)$(KDIR),)
$(error "KDIR or CPU must be specified.")
endif

# Get any user defined KDIR-<names> or maybe even a hardcoded KDIR
-include KDIR_CONFIGURATION

# Define host system directory
KDIR-$(shell uname -m):=/lib/modules/$(shell uname -r)/build

ifeq ($(ARCH), arm)
# when compiling for ARM we're cross compiling
export CROSS_COMPILE ?= $(call check_cc2, arm-linux-gnueabi-gcc, arm-linux-gnueabi-, arm-none-linux-gnueabi-)
endif

# look up KDIR based om CPU selection
KDIR ?= $(KDIR-$(CPU))

export CONFIG

export CONFIG_UMP := m
ifeq ($(BUILD),debug)
export CONFIG_UMP_DEBUG := y
else
export CONFIG_UMP_DEBUG := n
endif

ifeq ($(KDIR),)
$(error No KDIR found for platform $(CPU))
endif

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

kernelrelease:
	$(MAKE) -C $(KDIR) kernelrelease

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	$(MAKE) -C $(KDIR) M=$(CURDIR)/../mali clean
