/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_pp.h"
#include <linux/uaccess.h>

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side);

void *pp_get_driver_ops(struct mdp_pp_driver_ops *ops)
{
	if (!ops) {
		pr_err("PP driver ops invalid %p\n", ops);
		return ERR_PTR(-EINVAL);
	}

	/* IGC ops */
	ops->pp_ops[IGC].pp_set_config = NULL;
	ops->pp_ops[IGC].pp_get_config = NULL;

	/* PCC ops */
	ops->pp_ops[PCC].pp_set_config = NULL;
	ops->pp_ops[PCC].pp_get_config = NULL;

	/* GC ops */
	ops->pp_ops[GC].pp_set_config = NULL;
	ops->pp_ops[GC].pp_get_config = NULL;

	/* PA ops */
	ops->pp_ops[PA].pp_set_config = NULL;
	ops->pp_ops[PA].pp_get_config = NULL;

	/* Gamut ops */
	ops->pp_ops[GAMUT].pp_set_config = NULL;
	ops->pp_ops[GAMUT].pp_get_config = NULL;

	/* CSC ops */
	ops->pp_ops[CSC].pp_set_config = NULL;
	ops->pp_ops[CSC].pp_get_config = NULL;

	/* Dither ops */
	ops->pp_ops[DITHER].pp_set_config = NULL;
	ops->pp_ops[DITHER].pp_get_config = NULL;

	/* QSEED ops */
	ops->pp_ops[QSEED].pp_set_config = NULL;
	ops->pp_ops[QSEED].pp_get_config = NULL;

	/* PA_LUT ops */
	ops->pp_ops[HIST_LUT].pp_set_config = NULL;
	ops->pp_ops[HIST_LUT].pp_get_config = NULL;

	/* Set opmode pointers */
	ops->pp_opmode_config = pp_opmode_config;

	return NULL;
}

static void pp_opmode_config(int location, struct pp_sts_type *pp_sts,
		u32 *opmode, int side)
{
	if (!pp_sts || !opmode) {
		pr_err("Invalid pp_sts %p or opmode %p\n", pp_sts, opmode);
		return;
	}

	return;
}
