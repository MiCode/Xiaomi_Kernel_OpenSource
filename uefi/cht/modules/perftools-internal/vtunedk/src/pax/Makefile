#
# Version: 1.5
#
# Copyright (C) 2009-2014 Intel Corporation.  All Rights Reserved.
#
#     This file is part of SEP Development Kit
#
#     SEP Development Kit is free software; you can redistribute it
#     and/or modify it under the terms of the GNU General Public License
#     version 2 as published by the Free Software Foundation.
#
#     SEP Development Kit is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with SEP Development Kit; if not, write to the Free Software
#     Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#     As a special exception, you may use this file as part of a free software
#     library without restriction.  Specifically, if other files instantiate
#     templates or use macros or inline functions from this file, or you compile
#     this file and link it with other files to produce an executable, this
#     file does not by itself cause the resulting executable to be covered by
#     the GNU General Public License.  This exception does not however
#     invalidate any other reasons why the executable file might be covered by
#     the GNU General Public License.
#

# -------------------- user configurable options ------------------------

# base name of driver
DRIVER_NAME = pax

# location to install driver
INSTALL = .

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
EXTRA_CFLAGS += -I$(M)/../../include -I$(M)/../inc -DDRV_ANDROID

# if ARCH variable is set, unset it to avoid conflicts with kbuild
unexport ARCH

# platform details
MACH ?= i686
ifeq ($(MACH),x86_64)
PLATFORM=x32_64
endif
ifeq ($(PLATFORM),)
PLATFORM=x32
endif

# eventual filename of the driver
DRIVER_FILENAME=$(DRIVER_NAME).ko

# build options ...
ifneq ($(KERNELRELEASE),)
	obj-m := $(DRIVER_NAME).o

# targets ...

# Otherwise, we were called directly from the command
# line; invoke the kernel build system.
else
        PWD := $(shell pwd)
        KERNEL_SRC_DIR := $(PWD)/../../../../../../$(KERNEL_SRC_DIR)

all: default

default:
	$(MAKE) -C $(KERNEL_SRC_DIR) M=$(PWD) LDDINCDIR=$(PWD)/../../include modules PWD=$(PWD)

endif

install:
	@cp $(DRIVER_NAME).ko $(INSTALL)/$(DRIVER_FILENAME)
	@echo "Installed $(DRIVER_NAME) driver to $(INSTALL)/$(DRIVER_FILENAME) ."

clean:
	@rm -f *.o .*.o.cmd .*.o.d .*.ko.cmd .*.ko.unsigned.cmd *.gcno
	@rm -f $(DRIVER_NAME).ko $(DRIVER_NAME).ko.unsigned $(DRIVER_FILENAME)
	@rm -f Module.symvers Modules.symvers modules.order Module.markers
	@rm -rf .tmp_versions

distclean: clean
	@rm -f $(DRIVER_NAME)*.ko
