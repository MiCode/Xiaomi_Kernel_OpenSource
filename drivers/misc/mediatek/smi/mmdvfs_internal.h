/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MMDVFS_INTERNAL_H__
#define __MMDVFS_INTERNAL_H__

#include "mmdvfs_mgr.h"
#include <mt-plat/aee.h>

#define MMDVFS_LOG_TAG	"MMDVFS"

#define MMDVFSMSG(string, args...) pr_debug(string, ##args)

#define MMDVFSDEBUG(level, x...)	\
	do {				\
		if (mmdvfs_debug_level_get() >= level) \
			MMDVFSMSG(x);	\
	} while (0)

#ifdef CONFIG_MTK_AEE_FEATURE
#define MMDVFSERR(string, args...)	\
	do {				\
		pr_notice("error: "string, ##args);	\
		aee_kernel_warning(MMDVFS_LOG_TAG, "error: "string, ##args); \
	} while (0)
#else
#define MMDVFSERR(string, args...) pr_notice("error: "string, ##args)
#endif

extern void mmdvfs_internal_handle_state_change(
	struct mmdvfs_state_change_event *event);
extern void mmdvfs_internal_notify_vcore_calibration(
	struct mmdvfs_prepare_event *event);
extern int mmdvfs_internal_set_vpu_step(int current_step, int update_step);
#endif				/* __MMDVFS_INTERNAL_H__ */
