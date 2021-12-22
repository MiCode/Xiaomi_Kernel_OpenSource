/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */
#include <linux/pm_domain.h>
#include <linux/regmap.h>

#define CLK_NULL		0
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

enum PWR_STA_TYPE {
	PWR_STA,
	PWR_STA2,
	PWR_CON_STA,
	XPU_PWR_STA,
	XPU_PWR_STA2,
	OTHER_STA,
	STA_NUM,
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

struct pd_sta {
	int pd_id;
	enum PWR_STA_TYPE sta_type;
	u32 pwr_mask;
};

struct regbase {
	u32 phys;
	void __iomem *virt;
	int id;
	const char *name;
	int pg;
	const char *pn;
};

struct regname {
	struct regbase *base;
	int id;
	u32 ofs;
	const char *name;
};

struct mtk_vf {
	const char *name;
	int freq_table[4];
};

struct clkchk_ops {
	const struct regname *(*get_all_regnames)(void);
	u32 *(*get_spm_pwr_status_array)(void);
	struct pvd_msk *(*get_pvd_pwr_mask)(void);
	const char * const *(*get_off_pll_names)(void);
	const char * const *(*get_notice_pll_names)(void);
	bool (*is_pll_chk_bug_on)(void);
	struct mtk_vf *(*get_vf_table)(void);
	int (*get_vcore_opp)(void);
	bool (*is_pwr_on)(struct provider_clk *pvdck);
	void (*devapc_dump)(void);
	void (*dump_hwv_history)(struct regmap *regmap, u32 id);
	void (*get_bus_reg)(void);
	void (*dump_bus_reg)(struct regmap *regmap, u32 ofs);
	void (*dump_hwv_pll_reg)(struct regmap *regmap, u32 shift);
	bool (*is_cg_chk_pwr_on)(void);
};

int clkchk_pvdck_is_on(struct provider_clk *pvdck);
bool clkchk_pvdck_is_prepared(struct provider_clk *pvdck);
bool clkchk_pvdck_is_enabled(struct provider_clk *pvdck);
bool is_valid_reg(void __iomem *addr);
int set_clkchk_notify(void);
void set_clkchk_ops(const struct clkchk_ops *ops);

extern bool pdchk_get_bug_on_stat(void);
extern const struct dev_pm_ops clk_chk_dev_pm_ops;