/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APUSYS_MNOC_DBG_H__
#define __APUSYS_MNOC_DBG_H__

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#define mnoc_aee_warn(key, format, args...) \
	do { \
		pr_info(format, ##args); \
		aee_kernel_warning("MNOC", \
			"\nCRDISPATCH_KEY:" key "\n" format, ##args); \
	} while (0)
#else
#define mnoc_aee_warn(key, format, args...)
#endif

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
int create_debugfs(struct dentry *root);
void remove_debugfs(void);
#else
static inline int create_debugfs(struct dentry *root)
{
	return 0;
}
static inline void remove_debugfs(void)
{
}
#endif

#endif
