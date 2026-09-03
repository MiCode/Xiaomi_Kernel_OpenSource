/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright 2023 Unisoc(Shanghai) Technologies Co.Ltd
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation; version 2.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef _UNIBINDER_H
#define _UNIBINDER_H

#define pr_fmt(fmt) "unisoc_binder: " fmt

#include <linux/list.h>
#include <vdso/bits.h>
#include <../drivers/android/binder_internal.h>

/**
 * Set the thread flags, the flags indicate that the thread will enable or disable some features.
 * @pid:     the pid of the thread which will be attach or detach the feature flags
 * @flags:   use this param to set the feature flags, the flags please
 *           refer to @unibinder_feature_flags enum.
 * @set:     1 for attach feature flags, 0 for detach feature flags, other value will be defined in
 *           the future.
 */
void set_thread_flags(int pid, int flags, int set);

/**
 * These are the unisoc binder feature flags, it be used to adjust the binder mechanism for
 * binder thread.
 */
enum unibinder_feature {
	UB_FEATURE_DEFAULT,
	UB_FEATURE_SCHED_SKIP_RESTORE,
	UB_FEATURE_SCHED_SKIP_RESTORE_RESET,
	UB_FEATURE_SCHED_INHERIT_RT,

	__UB_FEATURE_MAX
};
/**
 * set this feature to enabled the default unisoc binder feature
 */
#define UBFF_DEFAULT                     (BIT(UB_FEATURE_DEFAULT))
/* set this flag will skip priority restore flow for binder thread, work for binder thread. */
#define UBFF_SCHED_SKIP_RESTORE          (BIT(UB_FEATURE_SCHED_SKIP_RESTORE))
/* set this flag to restore the prio state to normal */
#define UBFF_SCHED_SKIP_RESTORE_RESET    (BIT(UB_FEATURE_SCHED_SKIP_RESTORE_RESET))
/**
 * set this flag will enable the inherit rt for binder thread.
 * this flag is for the caller thread, but affect the binder thread which will do the binder work
 * for the caller thread.
 * TODO: design and implement the inherit rt feature
 */
#define UBFF_SCHED_INHERIT_RT            (BIT(UB_FEATURE_SCHED_INHERIT_RT))
/* the full feature bit mask, used to check the request flags are valid */
#define UBFF_FULL_MASK                   (BIT(__UB_FEATURE_MAX) - 1)

/**
 * The flag to inidcate that the set thread priority request from which vendor hook,
 * And now it mainly used by the unibinder_set_priority tracepoint to print the set priority request
 * information.
 */
enum unibinder_set_priority_from {
	SET_FROM_RESTORE_PRIO_VH,
	SET_FROM_SET_PRIO_VH
};

#endif/* _UNIBINDER_H */
