/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_PD_CHK_H
#define __MTK_PD_CHK_H

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
	void (*dump_subsys_reg)(unsigned int pd_id);
	bool (*is_in_pd_list)(unsigned int id);
	void (*debug_dump)(unsigned int pd_id, unsigned int pwr_sta);
};

void pd_check_common_init(const struct pdchk_ops *ops);

extern struct clk *clk_chk_lookup(const char *name);

#endif /* __MTK_PD_CHK_H */
