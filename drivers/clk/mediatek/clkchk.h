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

struct pd_msk {
	int pd_id;
	enum PWR_STA_TYPE sta_type;
	u32 pwr_val;
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

struct clkchk_ops {
	const struct regname *(*get_all_regnames)(void);
	u32 *(*get_spm_pwr_status_array)(void);
	u32 (*get_spm_pwr_status)(u32 ofs);
	u32 (*get_pwr_status)(s32 idx);
	struct pvd_msk *(*get_pvd_pwr_mask)(void);
	int (*get_pvd_pwr_data_idx)(const char *pvdname);
	const char * const *(*get_off_pll_names)(void);
	const char * const *(*get_notice_pll_names)(void);
	bool (*is_pll_chk_bug_on)(void);
	const char *(*get_vf_name)(int id);
	int (*get_vf_opp)(int id, int opp);
	u32 (*get_vf_num)(void);
	int (*get_vcore_opp)(void);
	bool (*is_pwr_on)(struct provider_clk *pvdck);
	void (*devapc_dump)(void);
	void (*dump_hwv_history)(struct regmap *regmap, u32 id);
	void (*get_bus_reg)(void);
	void (*dump_bus_reg)(struct regmap *regmap, u32 ofs);
	void (*dump_pll_reg)(bool bug_on);
	bool (*is_cg_chk_pwr_on)(void);
	void (*trace_clk_event)(const char *name, unsigned int clk_sta);
	void (*trigger_trace_dump)(unsigned int enable);
	void (*check_hwv_irq_sta)(void);
	const char * const *(*get_bypass_pll_name)(void);
};

int pwr_hw_is_on(enum PWR_STA_TYPE type, s32 val);
int clkchk_pvdck_is_on(struct provider_clk *pvdck);
bool clkchk_pvdck_is_prepared(struct provider_clk *pvdck);
bool clkchk_pvdck_is_enabled(struct provider_clk *pvdck);
bool is_valid_reg(void __iomem *addr);
int mtk_clk_check_muxes(void);
int set_clkchk_notify(void);
void set_clkchk_ops(const struct clkchk_ops *ops);
void clkchk_hwv_irq_init(struct platform_device *pdev);

extern bool pdchk_get_bug_on_stat(void);
extern void pdchk_dump_trace_evt(void);
extern const struct dev_pm_ops clk_chk_dev_pm_ops;