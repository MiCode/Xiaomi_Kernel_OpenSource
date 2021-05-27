// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/tracepoint-defs.h>
#include <linux/string.h>
#include <linux/module.h>
#include "apu_tp.h"

#define apu_tp_foreach(tbl, t) \
	for ((t) = (tbl); (t) && (t)->name != NULL; (t)++)

static void apu_tp_lookup(struct tracepoint *tp, void *priv)
{
	struct apu_tp_tbl *t, *tbl;

	tbl = (struct apu_tp_tbl *)priv;

	apu_tp_foreach(tbl, t) {
		if (!strcmp(t->name, tp->name))
			t->tp = tp;
	}
}

/**
 * apu_tp_exit() - Release trace point table
 *
 * @apu_tp_tbl: trace point table
 *
 * Unregister all trace points listed in the given table from kernel.
 * Note: The last entry of the table must be "APU_TP_TBL_END".
 */
void apu_tp_exit(struct apu_tp_tbl *tbl)
{
	struct apu_tp_tbl *t;

	apu_tp_foreach(tbl, t) {
		if (!t->tp)
			continue;
		tracepoint_probe_unregister(t->tp, t->func, NULL);
		t->registered = false;
	}
}

/**
 * for_each_apu_tracepoint() - Iterate tracepoints of this module.
 *
 * @fct: callback function for each tracepoint
 * @priv: private data to be passed to fct.
 */
#ifdef MODULE
static void for_each_apu_tracepoint(
	void (*fct)(struct tracepoint *tp, void *priv),
	void *priv, struct module *mod)
{
	tracepoint_ptr_t *iter, *begin, *end;

	if (IS_ERR_OR_NULL(mod) || !fct)
		return;

	begin = mod->tracepoints_ptrs;
	end = mod->tracepoints_ptrs + mod->num_tracepoints;

	for (iter = begin; iter < end; iter++)
		fct(tracepoint_ptr_deref(iter), priv);
}
#endif

/**
 * apu_tp_init_mod() - Initialize trace point table
 * @mod: additional module to be searched
 * @apu_tp_tbl: trace point table
 *
 * Register all trace points listed in the given table to kernel.
 * Note: The last entry of the table must be "APU_TP_TBL_END".
 */
int apu_tp_init_mod(struct apu_tp_tbl *tbl, struct module *mod)
{
	struct apu_tp_tbl *t;

#ifdef MODULE
	for_each_apu_tracepoint(apu_tp_lookup, tbl, THIS_MODULE);
	for_each_apu_tracepoint(apu_tp_lookup, tbl, mod);
#else
	for_each_kernel_tracepoint(apu_tp_lookup, tbl);
#endif

	apu_tp_foreach(tbl, t) {
		if (!t->tp) {
			pr_info("%s: %s was not found\n", __func__, t->name);
			apu_tp_exit(tbl);  /* free registered entries */
			return -EINVAL;
		}
		tracepoint_probe_register(t->tp, t->func, NULL);
		t->registered = true;
	}
	return 0;
}

