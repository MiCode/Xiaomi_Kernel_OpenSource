/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_PD_CHK_H
#define __MTK_PD_CHK_H

#include "clkchk.h"

#define PD_PWR_ON	1
#define PD_PWR_OFF	0
#define SWCG(_name) {						\
		.name = _name,					\
	}

/*
 *	Before MTCMOS off procedure, perform the Subsys CGs sanity check.
 */
struct pd_check_swcg {
	struct clk *c;
	const char *name;
};

struct pdchk_ops {
	struct pd_check_swcg *(*get_subsys_cg)(unsigned int id);
	void (*suspend_cg_dump)(const char *name);
	void (*dump_subsys_reg)(unsigned int pd_id);
	bool (*is_in_pd_list)(unsigned int id);
	void (*debug_dump)(unsigned int pd_id, unsigned int pwr_sta);
	void (*log_dump)(unsigned int pd_id, unsigned int pwr_sta);
	struct pd_sta *(*get_pd_pwr_msk)(int pd_id);
	int *(*get_off_mtcmos_id)(void);
	int *(*get_notice_mtcmos_id)(void);
	bool (*is_mtcmos_chk_bug_on)(void);
	int *(*get_suspend_allow_id)(void);
	void (*trace_power_event)(unsigned int pd_id, unsigned int pwr_sta);
};

void pdchk_common_init(const struct pdchk_ops *ops);
int set_pdchk_notify(void);

extern const struct dev_pm_ops pdchk_dev_pm_ops;
extern struct clk *clk_chk_lookup(const char *name);
extern int pwr_hw_is_on(enum PWR_STA_TYPE type, u32 mask);

#endif /* __MTK_PD_CHK_H */
