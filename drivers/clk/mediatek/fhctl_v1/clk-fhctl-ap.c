// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Pierre Lee <pierre.lee@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/iopoll.h>
#include "clk-fhctl.h"

#define PERCENT_TO_DDSLMT(dds, percent_m10) \
	((((dds) * (percent_m10)) >> 5) / 100)


#define POSDIV_LSB 24
#define POSDIV_MASK 0x7
static int __mt_fh_hw_hopping(struct clk_mt_fhctl *fh,
				int pll_id, unsigned int new_dds,
				int postdiv)
{
	unsigned int dds_mask, con1_temp;
	unsigned int mon_dds = 0;
	int ret;
	struct clk_mt_fhctl_regs *fh_regs;
	struct clk_mt_fhctl_pll_data *pll_data;

	fh_regs = fh->fh_regs;
	pll_data = fh->pll_data;
	dds_mask = fh->pll_data->dds_mask;

	if (new_dds > dds_mask)
		return -EINVAL;

	/* 1. sync ncpo to DDS of FHCTL */
	writel((readl(fh_regs->reg_con1) & pll_data->dds_mask) |
			FH_FHCTLX_PLL_TGL_ORG, fh_regs->reg_dds);

	/* 2. enable DVFS and Hopping control */

	/* enable dvfs mode */
	fh_set_field(fh_regs->reg_cfg, FH_SFSTRX_EN, 1);
	/* enable hopping */
	fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_EN, 1);

	/* for slope setting. */

	writel(pll_data->slope0_value, fh_regs->reg_slope0);
	/* SLOPE1 is for MEMPLL */
	writel(pll_data->slope1_value, fh_regs->reg_slope1);

	/* 3. switch to hopping control */
	fh_set_field(fh_regs->reg_hp_en, (0x1U << pll_id),
						REG_HP_EN_FHCTL_CTR);

	/* 4. set DFS DDS */
	writel((new_dds) | (FH_FHCTLX_PLL_DVFS_TRI), fh_regs->reg_dvfs);

	/* 4.1 ensure jump to target DDS */
	/* Wait 1000 us until DDS stable */
	ret = readl_poll_timeout_atomic(fh_regs->reg_mon, mon_dds,
			(mon_dds&pll_data->dds_mask) == new_dds, 10, 1000);
	if (ret)
		pr_info("ERROR %s: target_dds=0x%x, mon_dds=0x%x",
			__func__, new_dds, (mon_dds&pll_data->dds_mask));

	if (postdiv == -1) {
		/* Don't change DIV for fhctl UT */
		con1_temp = readl(fh_regs->reg_con1) & (~dds_mask);
		con1_temp = (con1_temp |
				(readl(fh_regs->reg_mon) & dds_mask) |
				FH_XXPLL_CON1_PCWCHG);
	} else {
		con1_temp = readl(fh_regs->reg_con1) & (~dds_mask);
		con1_temp = con1_temp & ~(POSDIV_MASK << POSDIV_LSB);
		con1_temp = (con1_temp |
				(readl(fh_regs->reg_mon) & dds_mask) |
				(postdiv & POSDIV_MASK) << POSDIV_LSB |
				FH_XXPLL_CON1_PCWCHG);
	}

	/* 5. write back to ncpo */
	writel(con1_temp, fh_regs->reg_con1);

	/* 6. switch to APMIXEDSYS control */
	fh_set_field(fh_regs->reg_hp_en, BIT(pll_id),
				REG_HP_EN_APMIXEDSYS_CTR);


	return 0;
}


static int clk_mt_fh_hw_pll_init(struct clk_mt_fhctl *fh)
{
	struct clk_mt_fhctl_regs *fh_regs;
	struct clk_mt_fhctl_pll_data *pll_data;
	int pll_id;
	unsigned int mask;

	pr_debug("mt_fh_pll_init() start ");

	pll_id = fh->pll_data->pll_id;
	fh_regs = fh->fh_regs;
	pll_data = fh->pll_data;

	mask = 1 << pll_id;

	if (fh_regs == NULL) {
		pr_info("ERROR fh_reg (%d) is NULL", pll_id);
		return -EFAULT;
	}

	if (pll_data == NULL) {
		pr_info("ERROR pll_data (%d) is NULL", pll_id);
		return -EFAULT;
	}

	fh_set_field(fh_regs->reg_clk_con, mask, 1);

	/* Release software-reset to reset */
	fh_set_field(fh_regs->reg_rst_con, mask, 0);
	fh_set_field(fh_regs->reg_rst_con, mask, 1);

	writel(0x00000000, fh_regs->reg_cfg);	/* No SSC/FH enabled */
	writel(0x00000000, fh_regs->reg_updnlmt); /* clr all setting */
	writel(0x00000000, fh_regs->reg_dds);	/* clr all settings */

	/* Check default enable SSC */
	if (pll_data->pll_default_ssc_rate != 0) {
		// Default Enable SSC to 0~-N%;
		fh->hal_ops->pll_ssc_enable(fh, pll_data->pll_default_ssc_rate);
	}

	return 0;
}

static int clk_mt_fh_hw_pll_unpause(struct clk_mt_fhctl *fh)
{
	int pll_id;
	struct clk_mt_fhctl_regs *fh_regs;
	unsigned long flags = 0;


	pll_id = fh->pll_data->pll_id;
	fh_regs = fh->fh_regs;

	if (fh->pll_data->pll_type != FH_PLL_TYPE_CPU) {
		pr_info("%s not support unpause.", fh->pll_data->pll_name);
		return -EFAULT;
	}

	pr_debug("%s fh_pll_id:%d", __func__, pll_id);

	spin_lock_irqsave(fh->lock, flags);

	/* unpause  */
	fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_CFG_PAUSE, 0);

	spin_unlock_irqrestore(fh->lock, flags);

	return 0;
}

static int clk_mt_fh_hw_pll_pause(struct clk_mt_fhctl *fh)
{
	int pll_id;
	struct clk_mt_fhctl_regs *fh_regs;
	unsigned long flags = 0;

	pll_id = fh->pll_data->pll_id;
	fh_regs = fh->fh_regs;

	if (fh->pll_data->pll_type != FH_PLL_TYPE_CPU) {
		pr_info("%s not support pause.", fh->pll_data->pll_name);
		return -EFAULT;
	}

	pr_debug("%s fh_pll_id:%d", __func__, pll_id);

	spin_lock_irqsave(fh->lock, flags);

	/* pause  */
	fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_CFG_PAUSE, 1);

	spin_unlock_irqrestore(fh->lock, flags);

	return 0;
}

static int clk_mt_fh_hw_pll_ssc_disable(struct clk_mt_fhctl *fh)
{
	int pll_id;
	unsigned long flags = 0;
	struct clk_mt_fhctl_regs *fh_regs;
	struct clk_mt_fhctl_pll_data *pll_data;


	pll_id = fh->pll_data->pll_id;
	fh_regs = fh->fh_regs;
	pll_data = fh->pll_data;

	if (pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) {
		pr_info("%s not support SSC.", pll_data->pll_name);
		return -EPERM;
	}

	pr_debug("fh_pll_id:%d", pll_id);

	spin_lock_irqsave(fh->lock, flags);

	/* Set the relative registers */
	fh_set_field(fh_regs->reg_cfg, FH_FRDDSX_EN, 0);
	fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_EN, 0);

	/* Switch to APMIXEDSYS control */
	fh_set_field(fh_regs->reg_hp_en, BIT(pll_id),
				REG_HP_EN_APMIXEDSYS_CTR);

	spin_unlock_irqrestore(fh->lock, flags);

	/* Wait for DDS to be stable */
	udelay(30);

	return 0;
}

static int clk_mt_fh_hw_pll_ssc_enable(struct clk_mt_fhctl *fh, int ssc_rate)
{
	int pll_id;
	unsigned long flags = 0;
	unsigned int dds_mask;
	unsigned int updnlmt_val;
	struct clk_mt_fhctl_regs *fh_regs;
	struct clk_mt_fhctl_pll_data *pll_data;


	pll_id = fh->pll_data->pll_id;
	fh_regs = fh->fh_regs;
	pll_data = fh->pll_data;
	dds_mask = fh->pll_data->dds_mask;

	if (pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) {
		pr_info("%s not support SSC.", pll_data->pll_name);
		return -EPERM;
	}

	pr_debug("pll_id:%d ssc:0~-%d%%", pll_id, ssc_rate);

	spin_lock_irqsave(fh->lock, flags);

	/* Set the relative parameter registers (dt/df/upbnd/downbnd) */
	fh_set_field(fh_regs->reg_cfg, MASK_FRDDSX_DYS, REG_CFG_DF_VAL);
	fh_set_field(fh_regs->reg_cfg, MASK_FRDDSX_DTS, REG_CFG_DT_VAL);

	writel((readl(fh_regs->reg_con1) & pll_data->dds_mask) |
			FH_FHCTLX_PLL_TGL_ORG, fh_regs->reg_dds);

	/* Calculate UPDNLMT */
	updnlmt_val = PERCENT_TO_DDSLMT((readl(fh_regs->reg_dds) &
					dds_mask), ssc_rate) << 16;

	writel(updnlmt_val, fh_regs->reg_updnlmt);

	/* Switch to FHCTL_CORE controller - Original design */
	fh_set_field(fh_regs->reg_hp_en, (0x1U << pll_id),
					REG_HP_EN_FHCTL_CTR);

	/* Enable SSC */
	fh_set_field(fh_regs->reg_cfg, FH_FRDDSX_EN, 1);
	/* Enable Hopping control */
	fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_EN, 1);

	/* Keep last ssc rate */
	fh->pll_data->pll_default_ssc_rate = ssc_rate;

	spin_unlock_irqrestore(fh->lock, flags);

	return 0;
}

static int clk_mt_fh_hw_pll_hopping(struct clk_mt_fhctl *fh,
					unsigned int new_dds,
					int postdiv)
{
	int pll_id;
	int ret;
	unsigned long flags = 0;
	unsigned int dds_mask;
	unsigned int updnlmt_val;
	unsigned int must_restore_ssc = 0;
	struct clk_mt_fhctl_regs *fh_regs;
	struct clk_mt_fhctl_pll_data *pll_data;


	pll_id = fh->pll_data->pll_id;
	fh_regs = fh->fh_regs;
	pll_data = fh->pll_data;
	dds_mask = pll_data->dds_mask;

	if ((fh->pll_data->pll_type == FH_PLL_TYPE_NOT_SUPPORT) ||
		(fh->pll_data->pll_type == FH_PLL_TYPE_CPU)) {
		pr_info("%s not support hopping in AP side",
						pll_data->pll_name);
		return -EPERM;
	}

	pr_debug("fh_pll_id:%d", pll_id);

	spin_lock_irqsave(fh->lock, flags);

	/* Check SSC status. */
	fh_get_field(fh_regs->reg_cfg, FH_FRDDSX_EN, must_restore_ssc);
	if (must_restore_ssc) {
		unsigned int pll_dds = 0;
		unsigned int mon_dds = 0;

		/* only when SSC is enable, turn off ARMPLL hopping */
		/* disable SSC mode */
		fh_set_field(fh_regs->reg_cfg, FH_FRDDSX_EN, 0);
		/* disable dvfs mode */
		fh_set_field(fh_regs->reg_cfg, FH_SFSTRX_EN, 0);
		/* disable hp ctl */
		fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_EN, 0);

		pll_dds = (readl(fh_regs->reg_dds)) & pll_data->dds_mask;

		/* Wait 1000 us until DDS stable */
		ret = readl_poll_timeout_atomic(fh_regs->reg_mon, mon_dds,
			(mon_dds&pll_data->dds_mask) == pll_dds, 10, 1000);
		if (ret)
			pr_info("ERROR %s: target_dds=0x%x, mon_dds=0x%x",
				__func__, pll_dds, mon_dds&pll_data->dds_mask);

	}

	ret = __mt_fh_hw_hopping(fh, pll_id, new_dds, postdiv);
	if (ret)
		pr_info("__mt_fh_hw_hopping error:%d", ret);


	/* Enable SSC status, if need. */
	if (must_restore_ssc) {
		/* disable SSC mode */
		fh_set_field(fh_regs->reg_cfg, FH_FRDDSX_EN, 0);
		/* disable dvfs mode */
		fh_set_field(fh_regs->reg_cfg, FH_SFSTRX_EN, 0);
		/* disable hp ctl */
		fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_EN, 0);

		fh_set_field(fh_regs->reg_cfg, MASK_FRDDSX_DYS, REG_CFG_DF_VAL);
		fh_set_field(fh_regs->reg_cfg, MASK_FRDDSX_DTS, REG_CFG_DT_VAL);

		writel((readl(fh_regs->reg_con1) & pll_data->dds_mask) |
				FH_FHCTLX_PLL_TGL_ORG, fh_regs->reg_dds);

		/* Calculate UPDNLMT */
		updnlmt_val = PERCENT_TO_DDSLMT(
				(readl(fh_regs->reg_dds) & dds_mask),
				pll_data->pll_default_ssc_rate) << 16;

		writel(updnlmt_val, fh_regs->reg_updnlmt);

		/* Switch to FHCTL_CORE controller - Original design */
		fh_set_field(fh_regs->reg_hp_en,
				(0x1U << pll_id), REG_HP_EN_FHCTL_CTR);

		/* enable SSC mode */
		fh_set_field(fh_regs->reg_cfg, FH_FRDDSX_EN, 1);
		/* enable hopping ctl */
		fh_set_field(fh_regs->reg_cfg, FH_FHCTLX_EN, 1);
	}

	spin_unlock_irqrestore(fh->lock, flags);

	return ret;
}

const struct clk_mt_fhctl_hal_ops mt_fhctl_hal_ops = {
	.pll_init = clk_mt_fh_hw_pll_init,
	.pll_unpause = clk_mt_fh_hw_pll_unpause,
	.pll_pause = clk_mt_fh_hw_pll_pause,
	.pll_ssc_disable = clk_mt_fh_hw_pll_ssc_disable,
	.pll_ssc_enable = clk_mt_fh_hw_pll_ssc_enable,
	.pll_hopping = clk_mt_fh_hw_pll_hopping,
};

