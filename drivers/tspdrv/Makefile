#=========================================================================
# File:
#    Makefile
#
# Description:
#    Makefile for TouchSense Linux Kernel Module
#    Targets Kernel 2.6
#
# Portions Copyright (c) 2008-2012 Immersion Corporation. All Rights Reserved.
# Copyright (C) 2015 XiaoMi, Inc.
#
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the GNU Public License v2 -
# (the 'License'). You may not use this file except in compliance with the
# License. You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
# TouchSenseSales@immersion.com.
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
