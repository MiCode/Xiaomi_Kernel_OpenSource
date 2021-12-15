// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MDW_DBG_H__
#define __APUSYS_MDW_DBG_H__

#include <apusys_dbg.h>

#define APUSYS_DBG_DIR "apusys_midware"

enum {
	MDW_DBG_PROP_MULTICORE,
	MDW_DBG_PROP_TCM_DEFAULT,
	MDW_DBG_PROP_QUERY_MEM,
	MDW_DBG_PROP_CMD_TIMEOUT_AEE,

	MDW_DBG_PROP_MAX,
};




void mdw_dbg_aee(char *name);
int mdw_dbg_get_prop(int idx);

int mdw_dbg_init(void);
int mdw_dbg_exit(void);

#endif
