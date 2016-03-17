#=========================================================================
# File:
#    Makefile
#
# Description:
#    Makefile for TouchSense Linux Kernel Module
#    Targets Kernel 2.6
#
# Portions Copyright (c) 2008-2012 Immersion Corporation. All Rights Reserved.
# Copyright (C) 2016 XiaoMi, Inc.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
# the License for the specific language governing rights and limitations
# under the License.
# =========================================================================

ifeq ($(KERNEL_ROOT),)
	override KERNEL_ROOT = /usr/src/linux
endif

EXTRA_CFLAGS += -I$(M)

obj-$(CONFIG_TSPDRV) += tspdrv.o

all:
	@$(MAKE) -C $(KERNEL_ROOT) M=$(shell pwd) modules

clean:
	@$(MAKE) -C $(KERNEL_ROOT) M=$(shell pwd) clean
