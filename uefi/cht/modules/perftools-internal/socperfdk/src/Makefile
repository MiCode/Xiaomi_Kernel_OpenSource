#
# Version: 1.0
#
#  This file is provided under a dual BSD/GPLv2 license.  When using or 
#  redistributing this file, you may do so under either license.
#
#  GPL LICENSE SUMMARY
#
#  Copyright(c) 2008-2014 Intel Corporation. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify 
#  it under the terms of version 2 of the GNU General Public License as
#  published by the Free Software Foundation.
#
#  This program is distributed in the hope that it will be useful, but 
#  WITHOUT ANY WARRANTY; without even the implied warranty of 
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License 
#  along with this program; if not, write to the Free Software 
#  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#  The full GNU General Public License is included in this distribution 
#  in the file called LICENSE.GPL.
#
#  BSD LICENSE 
#
#  Copyright(c) 2008-2014 Intel Corporation. All rights reserved.
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without 
#  modification, are permitted provided that the following conditions 
#  are met:
#
#    * Redistributions of source code must retain the above copyright 
#      notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above copyright 
#      notice, this list of conditions and the following disclaimer in 
#      the documentation and/or other materials provided with the 
#      distribution.
#    * Neither the name of Intel Corporation nor the names of its 
#      contributors may be used to endorse or promote products derived 
#      from this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
#  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
#  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
#  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
#  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
#  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
#  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
#  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# -------------------- user configurable options ------------------------

# base name of SocPerf driver
DRIVER_NAME = socperf1_2

# location to install driver
INSTALL = .

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
# Example flags are "-Werror", "-Wno-error", etc.
#EXTRA_CFLAGS += -I$(LDDINCDIR) -I$(LDDINCDIR1)
EXTRA_CFLAGS += -I$(M)/../include -I$(M)/inc

# if ARCH variable is set, unset it to avoid conflicts with kbuild
unexport ARCH

# platform details
MACH ?= i686
export MACH
ifeq ($(MACH),x86_64)
PLATFORM=x32_64
endif
ifeq ($(PLATFORM),)
PLATFORM=x32
endif

KERNEL_VERSION ?= $(shell uname -r)
SMP := $(shell uname -v | grep SMP)
ARITY=up
ifneq ($(SMP),)
ARITY=smp
endif

# eventual filename of SocPerf driver
DRIVER_MODE=$(DRIVER_NAME)
DRIVER_TYPE=$(PLATFORM)-$(KERNEL_VERSION)$(ARITY)
DRIVER_FILENAME=$(DRIVER_MODE)-$(DRIVER_TYPE).ko

# By default, build the EMON portions of the driver
# unless building IPF, which EMON doesn't support currently
EMON=YES

# build options ...
ifneq ($(KERNELRELEASE),)
	obj-m := $(DRIVER_NAME).o

ifeq ($(PLATFORM),x32)
	arch-objs :=    pci.o                 \
			soc_uncore.o                  \
			haswellunc_sa.o
endif
ifeq ($(PLATFORM),x32_64)
	arch-objs :=    pci.o                 \
			soc_uncore.o                  \
			haswellunc_sa.o
endif

ifeq ($(EMON),YES)
    EXTRA_CFLAGS += -DEMON -DEMON_INTERNAL
endif

EXTRA_CFLAGS += -DDRV_ANDROID -DDRV_ATOM_ONLY
# kernel APIs for PCI access
EXTRA_CFLAGS += -DPCI_HELPERS_API

	$(DRIVER_NAME)-objs :=    \
			socperfdrv.o   \
			control.o         \
			$(arch-objs)

# targets ...

# Otherwise, we were called directly from the command
# line, so the kernel build system will be used.
else
	KERNEL_SRC_DIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)

all: default

default:
	$(MAKE) -C $(KERNEL_SRC_DIR) M=$(PWD) LDDINCDIR=$(PWD)/../include LDDINCDIR1=$(PWD)/inc modules PWD=$(PWD)
	cp $(DRIVER_NAME).ko $(DRIVER_FILENAME)

endif

install:
	@cp $(DRIVER_NAME).ko $(INSTALL)/$(DRIVER_FILENAME)
	@echo "Installed $(DRIVER_NAME) driver to $(INSTALL)/$(DRIVER_FILENAME) ."

clean:
	rm -f *.o .*.o.cmd .*.o.d .*.ko.cmd .*.ko.unsigned.cmd *.gcno
	rm -f $(DRIVER_NAME).ko $(DRIVER_NAME).ko.unsigned
	rm -f $(DRIVER_MODE)*$(DRIVER_TYPE).ko
	rm -f Module.symvers Modules.symvers *.mod.c modules.order Module.markers
	rm -rf .tmp_versions

distclean: clean
	rm -f $(DRIVER_NAME)*.ko