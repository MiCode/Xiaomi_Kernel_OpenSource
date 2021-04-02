/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_POWER_DEBUG_H_
#define _APU_POWER_DEBUG_H_

#include "apupw_tag.h"
#include "apu_devfreq.h"

#define __LOG_BUF_LEN (1 << (CONFIG_LOG_BUF_SHIFT - 10))
#define LOG_LEN (__LOG_BUF_LEN - 1)

struct device;
struct regulator;

enum APUSYS_POWER_PARAM {
	POWER_PARAM_FIX_OPP,
	POWER_PARAM_DVFS_DEBUG,
	POWER_HAL_CTL,
	POWER_PARAM_SET_USER_OPP,
	POWER_PARAM_SET_THERMAL_OPP,
	POWER_PARAM_SET_POWER_HAL_OPP,
	POWER_PARAM_GET_POWER_REG,
	POWER_PARAM_POWER_STRESS,
	POWER_PARAM_OPP_TABLE,
	POWER_PARAM_CURR_STATUS,
	POWER_PARAM_LOG_LEVEL,
};

enum LOG_LEVEL {
	NO_LVL = 0,
	INFO_LVL,    /* show power info */
	VERBOSE_LVL, /* dump mux/sorting list */
	PERIOD_LVL,  /* per 100ms show power info */
	SHOW_LVL = 9,/* seq will show log level */
};

extern struct delayed_work pw_info_work;

/**
 * struct apu_dbg_clk - associate a clk with a notifier
 * @clk: struct clk * to associate the notifier with
 * @notifier_head: a blocking_notifier_head for this clk
 * @node: linked list pointers
 *
 * A list of struct clk_notifier is maintained by the notifier code.
 * An entry is created whenever code registers the first notifier on a
 * particular @clk.  Future notifiers on that @clk are added to the
 * @notifier_head.
 */
struct apu_dbg_clk {
	const char *name;
	struct clk	*clk;
	struct list_head node;
};

/**
 * struct apu_dbg_clk - associate a clk with a notifier
 * @clk: struct clk * to associate the notifier with
 * @notifier_head: a blocking_notifier_head for this clk
 * @node: linked list pointers
 *
 * A list of struct clk_notifier is maintained by the notifier code.
 * An entry is created whenever code registers the first notifier on a
 * particular @clk.  Future notifiers on that @clk are added to the
 * @notifier_head.
 */
struct apu_dbg_regulator {
	const char *name;
	struct regulator *reg;
	struct list_head node;
};

struct apu_dbg_cg {
	const char *name;
	void __iomem *reg;
	struct list_head node;
};

struct apu_dbg {
	struct list_head clk_list;
	struct list_head reg_list;
	struct list_head cg_list;

	/* global parameters */
	enum LOG_LEVEL log_lvl;
	int fix_opp;

	int option;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* below used for debugfs */
	struct dentry *dir;
	struct dentry *file;
	struct dentry *sym_link;
#endif
	int poll_interval;
};

int apupw_dbg_register_nodes(struct device *dev);
void apupw_dbg_release_nodes(void);
void apupw_dbg_power_info(struct work_struct *work);
enum LOG_LEVEL apupw_dbg_get_loglvl(void);
void apupw_dbg_set_loglvl(enum LOG_LEVEL lvl);
int apupw_dbg_get_fixopp(void);
void apupw_dbg_set_fixopp(int fix);
struct apupwr_tag *apupw_get_tag(void);
#endif
