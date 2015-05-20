###############################################################################
# -*- Mode: makefile -*-
#
# Copyright (C) 2010-2012 Intel Corporation.  All Rights Reserved.
#
# This file is part of SEP Development Kit
#
# SEP Development Kit is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# SEP Development Kit is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with SEP Development Kit; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#
# As a special exception, you may use this file as part of a free software
# library without restriction.  Specifically, if other files instantiate
# templates or use macros or inline functions from this file, or you compile
# this file and link it with other files to produce an executable, this
# file does not by itself cause the resulting executable to be covered by
# the GNU General Public License.  This exception does not however
# invalidate any other reasons why the executable file might be covered by
# the GNU General Public License.
#
###############################################################################

DRIVER_NAME := vtsspp

MARCH ?= $(shell uname -m | sed -e s/i.86/i386/)

# Check for external define from SEP Makefile
ifeq ($(KERNEL_VERSION),)
KVER  ?= $(shell uname -r)
else
KVER  ?= $(KERNEL_VERSION)
endif

# Check for external define from SEP Makefile
ifeq ($(KERNEL_SRC_DIR),)
KDIR  ?= /lib/modules/$(KVER)/build
else
KDIR  ?= $(KERNEL_SRC_DIR)
endif

###############################################################################
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)

obj-m := $(DRIVER_NAME).o
$(DRIVER_NAME)-objs := module.o collector.o procfs.o transport.o record.o \
                       task_map.o globals.o cpuevents.o user_vm.o stack.o \
                       apic.o dsa.o bts.o pebs.o lbr.o nmiwd.o ipt.o
ifeq ($(MARCH),i386)
EXTRA_CFLAGS += -DVTSS_ARCH_32
$(DRIVER_NAME)-objs += sys32.o
endif
ifeq ($(MARCH),x86_64)
ifneq ($(ARCH),)
DARCH := $(shell echo $(ARCH) | sed -e s/i386/32/ -e s/x86_// -e s/x86/64/ -e s/[lk]1om/KNX/)
EXTRA_CFLAGS += -DVTSS_ARCH_$(DARCH)
$(DRIVER_NAME)-objs += sys$(DARCH).o
else
EXTRA_CFLAGS += -DVTSS_ARCH_64
$(DRIVER_NAME)-objs += sys64.o
endif
endif

EXTRA_CFLAGS  += -Wno-unused -Werror
VTSS_FEATURES := $(subst :, ,$(VTSS))
ifneq ($(filter trace, $(VTSS_FEATURES)),)
EXTRA_CFLAGS  += -DVTSS_DEBUG_TRACE
endif
ifneq ($(filter profile, $(VTSS_FEATURES)),)
EXTRA_CFLAGS  += -DVTSS_DEBUG_PROFILE
endif
ifneq ($(filter uec, $(VTSS_FEATURES)),)
EXTRA_CFLAGS += -DVTSS_USE_UEC
$(DRIVER_NAME)-objs += uec.o
endif
ifneq ($(filter syscall, $(VTSS_FEATURES)),)
EXTRA_CFLAGS += -DVTSS_SYSCALL_TRACE
endif
ifneq ($(filter vma_cache, $(VTSS_FEATURES)),)
EXTRA_CFLAGS += -DVTSS_VMA_CACHE
endif
ifneq ($(filter android, $(VTSS_FEATURES)),)
EXTRA_CFLAGS += -DVTSS_CHECK_IP_IN_MAP -DVTSS_STACK_LIMIT=0x10000
endif
ifneq ($(filter mic, $(VTSS_FEATURES)),)
EXTRA_CFLAGS += -DVTSS_OVERFLOW_PAUSE -DVTSS_NO_BTS
endif

REQUIRED_SUBLEVEL := 32
CHECK_SUBLEVEL := $(shell if [ $(SUBLEVEL) -lt $(REQUIRED_SUBLEVEL) ] ; then echo less ; fi)
_KBUILD_CFLAGS := $(call flags,KBUILD_CFLAGS)
CHECK_BUILD    := $(CC) $(KBUILD_CPPFLAGS) $(CPPFLAGS) $(LINUXINCLUDE) \
                  $(_KBUILD_CFLAGS) $(CFLAGS_KERNEL) $(EXTRA_CFLAGS) $(CFLAGS) \
                  -DKBUILD_BASENAME=\"$(DRIVER_NAME)\" -Werror -S -o /dev/null -xc > /dev/null 2>&1
autoconf-srcs := $(sort $(wildcard $(KBUILD_SRC)/$(M)/autoconf/*.c))
$(addprefix $(obj)/,$($(DRIVER_NAME)-objs)): $(KBUILD_SRC)/$(M)/vtss_autoconf.h

$(KBUILD_SRC)/$(M)/vtss_autoconf.h: $(autoconf-srcs) $(KBUILD_SRC)/$(M)/Makefile
ifeq ($(VERSION).$(PATCHLEVEL),2.6)
	@$(if $(CHECK_SUBLEVEL),$(error Linux kernels prior to 2.6.$(REQUIRED_SUBLEVEL) are unsupported.))
else
ifneq ($(VERSION),3)
	@$(error Linux kernels prior to 2.6 are unsupported.)
endif
endif
	@echo "/**" > $@
	@echo " * Automatically generated file; DO NOT EDIT." >> $@
	@echo " * $(DRIVER_NAME).ko Configuration for $(KERNELRELEASE) Kernel Configuration" >> $@
	@echo " */" >> $@
	@echo "#ifndef _VTSS_AUTOCONF_H_" >> $@
	@echo "#define _VTSS_AUTOCONF_H_" >> $@
	@echo "" >> $@
	@echo "#if !defined(__i386__) && !defined(__x86_64__)" >> $@
	@echo "#error \"Only i386 or x86_64 architecture is supported\"" >> $@
	@echo "#endif" >> $@
	@echo "" >> $@
	@echo "#ifndef CONFIG_MODULES" >> $@
	@echo "#error \"The kernel should be compiled with CONFIG_MODULES=y\"" >> $@
	@echo "#endif /* CONFIG_MODULES */" >> $@
	@echo "" >> $@
	@echo "#ifndef CONFIG_MODULE_UNLOAD" >> $@
	@echo "#error \"The kernel should be compiled with CONFIG_MODULE_UNLOAD=y\"" >> $@
	@echo "#endif /* CONFIG_MODULE_UNLOAD */" >> $@
	@echo "" >> $@
	@echo "#ifndef CONFIG_SMP" >> $@
	@echo "#error \"The kernel should be compiled with CONFIG_SMP=y\"" >> $@
	@echo "#endif /* CONFIG_SMP */" >> $@
	@echo "" >> $@
	@echo "#ifndef CONFIG_KPROBES" >> $@
	@echo "#error \"The kernel should be compiled with CONFIG_KPROBES=y\"" >> $@
	@echo "#endif /* CONFIG_KPROBES */" >> $@
	@echo "" >> $@
	@$(foreach f, $(autoconf-srcs), if $(CHECK_BUILD) $(f); then (echo -n "#define "; echo -n VTSS_AUTOCONF_$(notdir $(f:.c=)) | tr '\-a-z' '_A-Z'; echo " 1") >> $@; fi;)
	@echo "" >> $@
	@if [ -f $(KBUILD_SRC)/$(M)/vtss_version.h ]; then \
		echo "#include \"vtss_version.h\""; \
		echo ""; \
	fi >> $@
	@echo "#ifndef VTSS_VERSION_MAJOR" >> $@
	@echo "#define VTSS_VERSION_MAJOR    1" >> $@
	@echo "#endif /* VTSS_VERSION_MAJOR */" >> $@
	@echo "#ifndef VTSS_VERSION_MINOR" >> $@
	@echo "#define VTSS_VERSION_MINOR    0" >> $@
	@echo "#endif /* VTSS_VERSION_MINOR */" >> $@
	@echo "#ifndef VTSS_VERSION_REVISION" >> $@
	@echo "#define VTSS_VERSION_REVISION 0" >> $@
	@echo "#endif /* VTSS_VERSION_REVISION */" >> $@
	@echo "#ifndef VTSS_VERSION_STRING" >> $@
	@echo "#define VTSS_VERSION_STRING   \"v1.0.0-custom\"" >> $@
	@echo "#endif /* VTSS_VERSION_STRING */" >> $@
	@echo "" >> $@
	@echo "#endif /* _VTSS_AUTOCONF_H_ */" >> $@

###############################################################################
# Otherwise, we were called directly from the command line;
# invoke the kernel build system.
else

PWD := $(shell pwd)
INSTALL ?= $(PWD)
MPLATFORM := unknown
ifeq ($(CROSS_COMPILE),)
ifeq ($(MARCH),i386)
MPLATFORM := x32
endif
ifeq ($(MARCH),x86_64)
MPLATFORM := x32_64
endif
else
ifneq ($(ARCH),)
MPLATFORM := $(ARCH)
endif
endif

# eventual filename of the driver
DRIVER_FILENAME := $(DRIVER_NAME)-$(MPLATFORM)-$(KVER)smp.ko

.PHONY: default all clean distclean install

default: all

all:
	-@$(MAKE) -C $(KDIR) M=$(PWD) V=0 modules
	-cp -f $(DRIVER_NAME).ko $(DRIVER_FILENAME)

clean:
	-@if [ -d $(KDIR) ]; then \
		$(MAKE) -C $(KDIR) M=$(PWD) V=0 clean; \
	else \
		rm -f *.o .*.o.cmd .*.o.d .*.ko.cmd .*.ko.unsigned.cmd *.gcno; \
	fi
	-rm -f $(PWD)/vtss_autoconf.h

distclean: clean
	-rm -f $(DRIVER_NAME).ko $(DRIVER_FILENAME)

install:
	-@cp -f $(DRIVER_NAME).ko $(INSTALL)/$(DRIVER_FILENAME)
	@echo "Installed $(DRIVER_NAME) driver to $(INSTALL)/$(DRIVER_FILENAME)"

endif
