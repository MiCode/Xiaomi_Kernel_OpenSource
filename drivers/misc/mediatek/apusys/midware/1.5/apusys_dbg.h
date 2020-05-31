/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DEBUG_H__
#define __APUSYS_DEBUG_H__

#define APUSYS_DBG_DIR "apusys_midware"

enum {
	DBG_PROP_MULTICORE,
	DBG_PROP_TCM_DEFAULT,
	DBG_PROP_QUERY_MEM,

	DBG_PROP_MAX,
};

int dbg_get_prop(int idx);
int apusys_dbg_init(struct dentry *root);
int apusys_dbg_destroy(void);

#endif
