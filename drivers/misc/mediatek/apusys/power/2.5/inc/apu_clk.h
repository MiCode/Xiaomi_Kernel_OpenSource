/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APU_CLK_H__
#define __APU_CLK_H__

#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/module.h>

#include "apu_devfreq.h"

enum CG_IDX {
	CG_CON = 0,
	CG_SET,
	CG_CLR,
	CG_REGS,
};

#define	POSDIV_SHIFT	(24)            /* bit */
#define	DDS0_SHIFT	(9)
#define	DDS1_SHIFT	(5)
#define	DDS_SHIFT	(14)            /* bit */
#define	PLL_FIN		(26)            /* MHz */

struct apu_cg {
	u32 cg_ctl[CG_REGS];
	phys_addr_t phyaddr;
	void __iomem *regs;
};

struct apu_cgs {
	struct apu_cg *cgs;
	struct device *dev;
	u32 clk_num;
};

struct apmixpll {
	ulong offset;
	ulong multiplier;
	void __iomem *regs;
};

struct apu_clk_parent {
	ulong rate;
	struct clk *clk;
};

struct apu_clk_gp;
struct apu_clk;

struct apu_clk {
	struct device *dev;
	/* for top/sys mux */
	struct clk_bulk_data *clks;
	struct apu_clk *parents;
	/* for apumix pll */
	struct apmixpll *mixpll;
	u32 clk_num;
	unsigned long def_freq;
	unsigned long shut_freq;

	/* apu_clk_gp flags */
	unsigned always_on:1;	/* clk never enable/disable */
	unsigned keep_enable:1;	/* enable once and never disable */
	unsigned fix_rate:1;		/* fix rate of all clks while enable/disable */
	unsigned dynamic_alloc:1;	/* allocated from dts */

} __aligned(sizeof(long));

/**
 * struct apu_clk_ops -  Callback operations for apu clocks.
 *
 * @enable:	Enable the clock atomically. This must not return until the
 *		clock is generating a valid clock signal, usable by consumer
 *		devices. Called with enable_lock held. This function must not
 *		sleep.
 *
 * @disable:	Disable the clock atomically. Called with enable_lock held.
 *		This function must not sleep.
 *
 * @set_rate:	Change the rate of this clock. The requested rate is specified
 *		by the second argument, which should typically be the return
 *		of .round_rate call.  The third argument gives the parent rate
 *		which is likely helpful for most .set_rate implementation.
 *		Returns 0 on success, -EERROR otherwise.
 *
 * @get_rate:   returns the closest rate actually supported by the clock.
 *
 */
struct apu_clk_ops {
	int		(*prepare)(struct apu_clk_gp *aclk);
	void    (*unprepare)(struct apu_clk_gp *aclk);
	int		(*enable)(struct apu_clk_gp *aclk);
	void	(*disable)(struct apu_clk_gp *aclk);
	int		(*cg_enable)(struct apu_clk_gp *aclk);
	void	(*cg_disable)(struct apu_clk_gp *aclk);
	int		(*cg_status)(struct apu_clk_gp *aclk, u32 *result);
	ulong	(*get_rate)(struct apu_clk_gp *aclk);
	int		(*set_rate)(struct apu_clk_gp *aclk, unsigned long rate);
};

struct apu_clk_gp {
	struct device *dev;
	struct mutex clk_lock;
	/* the top_mux/sys_mux/Pll for this composite clk */
	struct apu_clk *top_mux;
	struct apu_clk *sys_mux;
	struct apu_clk *top_pll;
	struct apu_clk *apmix_pll;
	struct apu_cgs *cg;
	/* the clk_hw/ops for this apu composite clk */
	struct apu_clk_ops	*ops;

	unsigned fhctl:1;	/* freq hopping or not */
};

struct apu_clk_array {
	char *name;
	struct apu_clk_gp *aclk_gp;
};

struct apu_clk_gp *clk_apu_get_clkgp(struct apu_dev *ad, const char *name);
void clk_apu_show_clk_info(struct apu_clk *dst, bool only_active);


#endif
