/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_DEBUG_H__
#define __APUSYS_DEBUG_H__

#define APUSYS_DBG_DIR "apusys_midware"

/* feature option */
enum {
	APUSYS_FO_MULTICORE,
	APUSYS_FO_SCHED,
	APUSYS_FO_PREEMPTION,
	APUSYS_FO_TIMERECORD,

	APUSYS_FO_MAX,
};

int apusys_dbg_create_queue(int *dev_type);
int get_fo_from_list(int idx);
int apusys_dbg_init(void);
int apusys_dbg_destroy(void);

#endif
