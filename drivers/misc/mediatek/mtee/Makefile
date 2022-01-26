#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# TZ KREE driver makefile.

# Kernel does not work with kcov instrumentation here.
# the problem observed was insertion of __sanitizer_cov_trace_pc()
# callback into middle of assemble embedded code. Thus the callback observed
# inconsistent state and crashed.
KCOV_INSTRUMENT_tz_system.o := n

ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/trustzone

obj-y += tz_module.o
tz_module-y += tz_mod.o tz_system.o test.o kree_mem.o tz_sys_ipc.o \
               tz_irq.o tz_pm.o tz_clock.o tz_ndbg.o tz_counter.o tz_mem.o tz_fileio.o tz_playready.o tz_secure_clock.o

