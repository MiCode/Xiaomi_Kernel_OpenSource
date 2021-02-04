/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __DRV_CLKDBG_H
#define __DRV_CLKDBG_H

struct seq_file;

#define TAG	"[clkdbg] "

#define clk_err(fmt, args...)	pr_err(TAG fmt, ##args)
#define clk_warn(fmt, args...)	pr_warn(TAG fmt, ##args)
#define clk_info(fmt, args...)	pr_debug(TAG fmt, ##args)
#define clk_dbg(fmt, args...)	pr_debug(TAG fmt, ##args)
#define clk_ver(fmt, args...)	pr_debug(TAG fmt, ##args)

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_setl(addr, val)	clk_writel(addr, clk_readl(addr) | (val))
#define clk_clrl(addr, val)	clk_writel(addr, clk_readl(addr) & ~(val))

struct fmeter_clk {
	u8 type;
	u8 id;
	const char *name;
};

struct regbase {
	u32 phys;
	void __iomem *virt;
	const char *name;
};

struct regname {
	struct regbase *base;
	u32 ofs;
	const char *name;
};

#define ADDR(rn)	(rn->base->virt + rn->ofs)
#define PHYSADDR(rn)	(rn->base->phys + rn->ofs)

struct cmd_fn {
	const char	*cmd;
	int (*fn)(struct seq_file *, void *);
};

#define CMDFN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

struct provider_clk {
	const char *provider_name;
	u32 idx;
	struct clk *ck;
	u32 pwr_mask;
};

struct clkdbg_ops {
	const struct fmeter_clk *(*get_all_fmeter_clks)(void);
	void *(*prepare_fmeter)(void);
	void (*unprepare_fmeter)(void *data);
	u32 (*fmeter_freq)(const struct fmeter_clk *fclk);
	const struct regname *(*get_all_regnames)(void);
	const char * const *(*get_all_clk_names)(void);
	const char * const *(*get_pwr_names)(void);
	void (*setup_provider_clk)(struct provider_clk *pvdck);
};

void set_clkdbg_ops(const struct clkdbg_ops *ops);
void set_custom_cmds(const struct cmd_fn *cmds);

struct provider_clk *get_all_provider_clks(void);
const char *get_last_cmd(void);

void reg_pdrv(const char *pdname);
void unreg_pdrv(const char *pdname);
void prepare_enable_provider(const char *pvd);
void disable_unprepare_provider(const char *pvd);

void print_regs(void);
void print_fmeter_all(void);

#endif /* __DRV_CLKDBG_H */
