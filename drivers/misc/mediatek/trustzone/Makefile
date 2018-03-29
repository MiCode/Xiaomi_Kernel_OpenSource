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

ccflags-y += -I$(srctree)/include -I$(srctree)/include/trustzone -Werror

# Build as module
#obj-m += tz_module.o
obj-y += tz_module.o
tz_module-y += tz_mod.o tz_system.o test.o kree_mem.o tz_sys_ipc.o \
               tz_irq.o tz_pm.o tz_clock.o tz_ndbg.o tz_counter.o tz_mem.o tz_fileio.o tz_playready.o tz_secure_clock.o
