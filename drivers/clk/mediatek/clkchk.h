/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#define PD_NULL		-1
#define VCORE_NULL	-1

struct seq_file;

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_setl(addr, val)	clk_writel(addr, clk_readl(addr) | (val))
#define clk_clrl(addr, val)	clk_writel(addr, clk_readl(addr) & ~(val))

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3},	\
	}

#define ADDR(rn)	(rn->base->virt + rn->ofs)
#define PHYSADDR(rn)	(rn->base->phys + rn->ofs)

enum FMETER_TYPE {
	FT_NULL,
	ABIST,
	CKGEN,
	ABIST_2,
};

enum PWR_STA_TYPE {
	PWR_STA,
	PWR_STA2,
	OTHER_STA,
	STA_NUM,
};

struct fmeter_clk {
	enum FMETER_TYPE type;
	u32 id;
	const char *name;
	u32 ofs;
	u32 pdn;
};

struct provider_clk {
	const char *provider_name;
	u32 idx;
	struct clk *ck;
	const char *ck_name;
	u32 pwr_mask;
	enum PWR_STA_TYPE sta_type;
};

struct pvd_msk {
	const char *pvdname;
	enum PWR_STA_TYPE sta_type;
	u32 pwr_mask;
};

struct regbase {
	u32 phys;
	void __iomem *virt;
	const char *name;
	int pg;
};

struct regname {
	struct regbase *base;
	u32 ofs;
	const char *name;
};

struct clkchk_ops {
	const struct regname *(*get_all_regnames)(void);
	struct pvd_msk *(*get_pvd_pwr_mask)(void);
	const char * const *(*get_off_pll_names)(void);
	const char * const *(*get_notice_pll_names)(void);
	bool (*is_pll_chk_bug_on)(void);
	const char *(*get_vf_name)(int id);
	int (*get_vf_opp)(int id, int opp);
	u32 (*get_vf_num)(void);
	int (*get_vcore_opp)(void);
	int *(*get_pwr_stat)(void);
};

void set_clkchk_ops(const struct clkchk_ops *ops);
void clkchk_set_cfg(void);

