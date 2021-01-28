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

#ifndef __APU_TP_H__
#define __APU_TP_H__

#include <linux/tracepoint-defs.h>

struct apu_tp_tbl {
	const char *name;  /* name of the trace point (TP) */
	void *func;  /* functional pointer to the TP hook */
	struct tracepoint *tp;  /* trace point looked-up from kernel */
	bool registered;  /* TP register status */
};

/* Last entry of the table must be APU_TP_TBL_END */
#define APU_TP_TBL_END  {.name = NULL, .func = NULL},

#ifdef CONFIG_MTK_APUSYS_DEBUG
int apu_tp_init(struct apu_tp_tbl *tbl);
void apu_tp_exit(struct apu_tp_tbl *tbl);

#else

static inline
int apu_tp_init(struct apu_tp_tbl *tbl)
{
	return 0;
}

static inline
void apu_tp_exit(struct apu_tp_tbl *tbl)
{

}
#endif

#endif
