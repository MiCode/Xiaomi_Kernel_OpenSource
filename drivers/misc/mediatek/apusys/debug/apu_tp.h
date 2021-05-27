/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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

#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)

int apu_tp_init_mod(struct apu_tp_tbl *tbl, struct module *mod);
void apu_tp_exit(struct apu_tp_tbl *tbl);

#else

static inline
int apu_tp_init_mod(struct apu_tp_tbl *tbl, struct module *mod)
{
	return 0;
}

static inline
void apu_tp_exit(struct apu_tp_tbl *tbl)
{
}

#endif

static inline
int apu_tp_init(struct apu_tp_tbl *tbl)
{
	return apu_tp_init_mod(tbl, NULL);
}

#endif
