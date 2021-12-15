// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APUSYS_MNOC_DBG_H__
#define __APUSYS_MNOC_DBG_H__

#include <aee.h>

#ifdef CONFIG_MTK_AEE_FEATURE
#define mnoc_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("MNOC", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define mnoc_aee_warn(key, format, args...)
#endif

int create_debugfs(void);
void remove_debugfs(void);

#endif
