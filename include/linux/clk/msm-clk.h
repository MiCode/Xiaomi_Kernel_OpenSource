/* Copyright (c) 2009, 2012-2014 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MACH_CLK_H
#define __MACH_CLK_H

#include <linux/notifier.h>

#define CLKFLAG_INVERT			0x00000001
#define CLKFLAG_NOINVERT		0x00000002
#define CLKFLAG_NONEST			0x00000004
#define CLKFLAG_NORESET			0x00000008
#define CLKFLAG_RETAIN_PERIPH		0x00000010
#define CLKFLAG_NORETAIN_PERIPH		0x00000020
#define CLKFLAG_RETAIN_MEM		0x00000040
#define CLKFLAG_NORETAIN_MEM		0x00000080
#define CLKFLAG_SKIP_HANDOFF		0x00000100
#define CLKFLAG_MIN			0x00000400
#define CLKFLAG_MAX			0x00000800
#define CLKFLAG_INIT_DONE		0x00001000
#define CLKFLAG_INIT_ERR		0x00002000
#define CLKFLAG_NO_RATE_CACHE		0x00004000
#define CLKFLAG_MEASURE			0x00008000
#define CLKFLAG_EPROBE_DEFER		0x00010000

struct clk_lookup;
struct clk;

enum clk_reset_action {
	CLK_RESET_DEASSERT	= 0,
	CLK_RESET_ASSERT	= 1
};

struct clk_src {
	struct clk *src;
	int sel;
};

/* Rate is maximum clock rate in Hz */
int clk_set_max_rate(struct clk *clk, unsigned long rate);

/* Assert/Deassert reset to a hardware block associated with a clock */
int clk_reset(struct clk *clk, enum clk_reset_action action);

/* Set clock-specific configuration parameters */
int clk_set_flags(struct clk *clk, unsigned long flags);

/* returns the mux selection index associated with a particular parent */
int parent_to_src_sel(struct clk_src *parents, int num_parents, struct clk *p);

/* returns the mux selection index associated with a particular parent */
int clk_get_parent_sel(struct clk *c, struct clk *parent);

/**
 * DOC: clk notifier callback types
 *
 * PRE_RATE_CHANGE - called immediately before the clk rate is changed,
 *     to indicate that the rate change will proceed.  Drivers must
 *     immediately terminate any operations that will be affected by the
 *     rate change.  Callbacks may either return NOTIFY_DONE, NOTIFY_OK,
 *     NOTIFY_STOP or NOTIFY_BAD.
 *
 * ABORT_RATE_CHANGE: called if the rate change failed for some reason
 *     after PRE_RATE_CHANGE.  In this case, all registered notifiers on
 *     the clk will be called with ABORT_RATE_CHANGE. Callbacks must
 *     always return NOTIFY_DONE or NOTIFY_OK.
 *
 * POST_RATE_CHANGE - called after the clk rate change has successfully
 *     completed.  Callbacks must always return NOTIFY_DONE or NOTIFY_OK.
 *
 */
#define PRE_RATE_CHANGE			BIT(0)
#define POST_RATE_CHANGE		BIT(1)
#define ABORT_RATE_CHANGE		BIT(2)

/**
 * struct msm_clk_notifier - associate a clk with a notifier
 * @clk: struct clk * to associate the notifier with
 * @notifier_head: a blocking_notifier_head for this clk
 * @node: linked list pointers
 *
 * A list of struct clk_notifier is maintained by the notifier code.
 * An entry is created whenever code registers the first notifier on a
 * particular @clk.  Future notifiers on that @clk are added to the
 * @notifier_head.
 */
struct msm_clk_notifier {
	struct clk			*clk;
	struct srcu_notifier_head	notifier_head;
	struct list_head		node;
};

/**
 * struct msm_clk_notifier_data - rate data to pass to the notifier callback
 * @clk: struct clk * being changed
 * @old_rate: previous rate of this clk
 * @new_rate: new rate of this clk
 *
 * For a pre-notifier, old_rate is the clk's rate before this rate
 * change, and new_rate is what the rate will be in the future.  For a
 * post-notifier, old_rate and new_rate are both set to the clk's
 * current rate (this was done to optimize the implementation).
 */
struct msm_clk_notifier_data {
	struct clk		*clk;
	unsigned long		old_rate;
	unsigned long		new_rate;
};

int msm_clk_notif_register(struct clk *clk, struct notifier_block *nb);

int msm_clk_notif_unregister(struct clk *clk, struct notifier_block *nb);

#endif
