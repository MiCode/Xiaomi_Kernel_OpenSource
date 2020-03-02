# Copyright (c) 2015-2018 TrustKernel Incorporated
# All Rights Reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

ifneq ($(strip $(M)),)
	obj-m += tkcore.o
else
	obj-y += tkcore.o
endif

M ?=$(srctree)/drivers/misc/mediatek/tkcore

#ccflags-y+=-Werror
ccflags-y += -I$(M)/include/linux
ccflags-y += -I$(M)/include
ccflags-y += -I$(M)

tkcore-objs:= \
		tee_core.o \
		tee_context.o \
		tee_session.o \
		tee_shm.o \
		tee_supp_com.o \
		tee_sysfs.o \
		tee_kernel_api.o \
		tee_wait_queue.o \
		tee_tui.o \
		tee_tui_hal.o \
		tee_procfs.o \
		tee_ta_mgmt.o \
		tee_clkmgr.o \
		tee_fp.o \
		pm.o \
