#
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See http://www.gnu.org/licenses/gpl-2.0.html for more details.
#

subdir-ccflags-y += -Idrivers/misc/mediatek/mtprof/

ifdef CONFIG_MTK_AEE_IPANIC
ccflags-y += -DTEXT_OFFSET=$(TEXT_OFFSET)
obj-y	+= mrdump.o

mrdump-objs-y := \
	mrdump_control.o \
	mrdump_hw.o \
	mrdump_full.o \
	mrdump_mini.o \
	mrdump_panic.o

ifndef CONFIG_KEXEC_CORE
mrdump-objs-y += mrdump_key_setup.o
endif
mrdump-objs-$(CONFIG_ARM) += mrdump_arm.o
mrdump-objs-$(CONFIG_ARM_PSCI) += mrdump_panic_wdt.o

mrdump-objs-$(CONFIG_ARM64) += \
	mrdump_arm64.o \
	mrdump_panic_wdt.o

mrdump-objs := $(mrdump-objs-y)

KASAN_SANITIZE := n
endif
