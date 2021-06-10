/*
 * Copyright (C) 2020 MediaTek Inc.
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
