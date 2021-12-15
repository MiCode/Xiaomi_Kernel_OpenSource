/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <stdbool.h>
#include <stddef.h>

#define VCORE_NULL	-1
#define ADDR(rn)	(rn->base->virt + rn->ofs)
#define PHYSADDR(rn)	(rn->base->phys + rn->ofs)
#define SWCG(_name) {						\
		.name = _name,					\
	}

/*
 *	Before MTCMOS off procedure, perform the Subsys CGs sanity check.
 */
struct pg_check_swcg {
	struct clk *c;
	const char *name;
};




struct clkchk_op {
	const char *(*get_vf_name)(int id);
	int (*get_vf_opp)(int id, int opp);
	int (*get_vf_num)(void);
	int (*get_vcore_opp)(void);
	void (*get_devapc_dump)(void);
};


struct clkchk_cfg_t {
	bool aee_excp_on_fail;
	bool bug_on_fail;
	bool warn_on_fail;
	const char * const *compatible;
	const char * const *off_pll_names;
	const char * const *off_mtcmos_names;
	const char * const *notice_pll_names;
	const char * const *notice_mtcmos_names;
	const char * const *all_clk_names;
	//struct clkchk_op *ops;
	const char *(*get_vf_name)(int id);
	int (*get_vf_opp)(int id, int opp);
	u32 (*get_vf_num)(void);
	int (*get_vcore_opp)(void);
	void (*get_devapc_dump)(void);
};

void dump_enabled_clks_once(void);
bool is_valid_reg(void __iomem *addr);
void __init clkchk_swcg_init(struct pg_check_swcg *swcg);
int clkchk_init(struct clkchk_cfg_t *cfg);
