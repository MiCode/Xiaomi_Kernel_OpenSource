/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <stdbool.h>
#include <stddef.h>

struct clkchk_cfg_t {
	bool aee_excp_on_fail;
	bool warn_on_fail;
	const char * const *compatible;
	const char * const *off_pll_names;
	const char * const *all_clk_names;
};

int clkchk_init(struct clkchk_cfg_t *cfg);
