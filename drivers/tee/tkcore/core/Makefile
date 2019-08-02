# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2015-2019 TrustKernel Incorporated

ifneq ($(strip $(M)),)
	obj-m += tkcore.o
else
	obj-y += tkcore.o
endif

M ?=$(srctree)/drivers/tee/tkcore

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
