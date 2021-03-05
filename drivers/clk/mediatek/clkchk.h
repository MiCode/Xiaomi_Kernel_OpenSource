/*
 * Copyright (C) 2017 MediaTek Inc.
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

struct regbase {
	u32 phys;
	void __iomem *virt;
	const char *name;
	const char *pg;
};

struct regname {
	struct regbase *base;
	u32 ofs;
	const char *name;
};

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN,
	ABIST_2,
	SUBSYS,
};

struct fmeter_clk {
	enum FMETER_TYPE type;
	u32 id;
	const char *name;
	u32 ofs;
	u32 pdn;
	u32 grp;
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
