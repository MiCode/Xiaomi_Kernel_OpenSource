/*
 * drivers/video/tegra/dc/sor.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/nvhost.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/clk/tegra.h>

#include <mach/dc.h>

#include "sor.h"
#include "sor_regs.h"
#include "dc_priv.h"

#include "../../../../arch/arm/mach-tegra/iomap.h"

#define APBDEV_PMC_DPD_SAMPLE				(0x20)
#define APBDEV_PMC_DPD_SAMPLE_ON_DISABLE		(0)
#define APBDEV_PMC_DPD_SAMPLE_ON_ENABLE			(1)
#define APBDEV_PMC_SEL_DPD_TIM				(0x1c8)
#define APBDEV_PMC_SEL_DPD_TIM_SEL_DPD_TIM_DEFAULT	(0x7f)
#define APBDEV_PMC_IO_DPD2_REQ				(0x1c0)
#define APBDEV_PMC_IO_DPD2_REQ_LVDS_SHIFT		(25)
#define APBDEV_PMC_IO_DPD2_REQ_LVDS_OFF			(0 << 25)
#define APBDEV_PMC_IO_DPD2_REQ_LVDS_ON			(1 << 25)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_SHIFT               (30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_DEFAULT_MASK        (0x3 << 30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_IDLE                (0 << 30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_OFF             (1 << 30)
#define APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_ON              (2 << 30)
#define APBDEV_PMC_IO_DPD2_STATUS			(0x1c4)
#define APBDEV_PMC_IO_DPD2_STATUS_LVDS_SHIFT		(25)
#define APBDEV_PMC_IO_DPD2_STATUS_LVDS_OFF		(0 << 25)
#define APBDEV_PMC_IO_DPD2_STATUS_LVDS_ON		(1 << 25)

static u32 tegra_dc_sor_poll_register(struct tegra_dc_sor_data *sor,
	u32 reg, u32 mask, u32 exp_val, u32 poll_interval_us, u32 timeout_ms)
{
	unsigned long timeout_jf = jiffies + msecs_to_jiffies(timeout_ms);
	u32 reg_val = 0;

	do {
		usleep_range(poll_interval_us, poll_interval_us << 1);
		reg_val = tegra_sor_readl(sor, reg);
	} while (((reg_val & mask) != exp_val) &&
		time_after(timeout_jf, jiffies));

	if ((reg_val & mask) == exp_val)
		return 0;	/* success */
	dev_err(&sor->dc->ndev->dev,
		"sor_poll_register 0x%x: timeout\n", reg);
	return jiffies - timeout_jf + 1;
}


#ifdef CONFIG_DEBUG_FS
static int dbg_sor_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_sor_data *sor = s->private;

#define DUMP_REG(a) seq_printf(s, "%-32s  %03x  %08x\n",		\
		#a, a, tegra_sor_readl(sor, a));

	tegra_dc_io_start(sor->dc);
	clk_prepare_enable(sor->sor_clk);

	DUMP_REG(NV_SOR_SUPER_STATE0);
	DUMP_REG(NV_SOR_SUPER_STATE1);
	DUMP_REG(NV_SOR_STATE0);
	DUMP_REG(NV_SOR_STATE1);
	DUMP_REG(NV_HEAD_STATE0(0));
	DUMP_REG(NV_HEAD_STATE0(1));
	DUMP_REG(NV_HEAD_STATE1(0));
	DUMP_REG(NV_HEAD_STATE1(1));
	DUMP_REG(NV_HEAD_STATE2(0));
	DUMP_REG(NV_HEAD_STATE2(1));
	DUMP_REG(NV_HEAD_STATE3(0));
	DUMP_REG(NV_HEAD_STATE3(1));
	DUMP_REG(NV_HEAD_STATE4(0));
	DUMP_REG(NV_HEAD_STATE4(1));
	DUMP_REG(NV_HEAD_STATE5(0));
	DUMP_REG(NV_HEAD_STATE5(1));
	DUMP_REG(NV_SOR_CRC_CNTRL);
	DUMP_REG(NV_SOR_CLK_CNTRL);
	DUMP_REG(NV_SOR_CAP);
	DUMP_REG(NV_SOR_PWR);
	DUMP_REG(NV_SOR_TEST);
	DUMP_REG(NV_SOR_PLL0);
	DUMP_REG(NV_SOR_PLL1);
	DUMP_REG(NV_SOR_PLL2);
	DUMP_REG(NV_SOR_PLL3);
	DUMP_REG(NV_SOR_CSTM);
	DUMP_REG(NV_SOR_LVDS);
	DUMP_REG(NV_SOR_CRCA);
	DUMP_REG(NV_SOR_CRCB);
	DUMP_REG(NV_SOR_SEQ_CTL);
	DUMP_REG(NV_SOR_LANE_SEQ_CTL);
	DUMP_REG(NV_SOR_SEQ_INST(0));
	DUMP_REG(NV_SOR_SEQ_INST(1));
	DUMP_REG(NV_SOR_SEQ_INST(2));
	DUMP_REG(NV_SOR_SEQ_INST(3));
	DUMP_REG(NV_SOR_SEQ_INST(4));
	DUMP_REG(NV_SOR_SEQ_INST(5));
	DUMP_REG(NV_SOR_SEQ_INST(6));
	DUMP_REG(NV_SOR_SEQ_INST(7));
	DUMP_REG(NV_SOR_SEQ_INST(8));
	DUMP_REG(NV_SOR_PWM_DIV);
	DUMP_REG(NV_SOR_PWM_CTL);
	DUMP_REG(NV_SOR_MSCHECK);
	DUMP_REG(NV_SOR_XBAR_CTRL);
	DUMP_REG(NV_SOR_DP_LINKCTL(0));
	DUMP_REG(NV_SOR_DP_LINKCTL(1));
	DUMP_REG(NV_SOR_DC(0));
	DUMP_REG(NV_SOR_DC(1));
	DUMP_REG(NV_SOR_LANE_DRIVE_CURRENT(0));
	DUMP_REG(NV_SOR_PR(0));
	DUMP_REG(NV_SOR_LANE4_PREEMPHASIS(0));
	DUMP_REG(NV_SOR_POSTCURSOR(0));
	DUMP_REG(NV_SOR_DP_CONFIG(0));
	DUMP_REG(NV_SOR_DP_CONFIG(1));
	DUMP_REG(NV_SOR_DP_MN(0));
	DUMP_REG(NV_SOR_DP_MN(1));
	DUMP_REG(NV_SOR_DP_PADCTL(0));
	DUMP_REG(NV_SOR_DP_PADCTL(1));
	DUMP_REG(NV_SOR_DP_DEBUG(0));
	DUMP_REG(NV_SOR_DP_DEBUG(1));
	DUMP_REG(NV_SOR_DP_SPARE(0));
	DUMP_REG(NV_SOR_DP_SPARE(1));
	DUMP_REG(NV_SOR_DP_TPG);

	clk_disable_unprepare(sor->sor_clk);
	tegra_dc_io_end(sor->dc);

	return 0;
}

static int dbg_sor_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_sor_show, inode->i_private);
}

static const struct file_operations dbg_fops = {
	.open		= dbg_sor_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *sordir;

static void tegra_dc_sor_debug_create(struct tegra_dc_sor_data *sor)
{
	struct dentry *retval;

	sordir = debugfs_create_dir("tegra_sor", NULL);
	if (!sordir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, sordir, sor, &dbg_fops);
	if (!retval)
		goto free_out;
	return;
free_out:
	debugfs_remove_recursive(sordir);
	sordir = NULL;
	return;
}
#else
static inline void tegra_dc_sor_debug_create(struct tegra_dc_sor_data *sor)
{ }
#endif


struct tegra_dc_sor_data *tegra_dc_sor_init(struct tegra_dc *dc,
	const struct tegra_dc_dp_link_config *cfg)
{
	struct tegra_dc_sor_data	*sor;
	struct resource			*res;
	struct resource			*base_res;
	void __iomem			*base;
	struct clk			*clk;
	int				 err;

	sor = kzalloc(sizeof(*sor), GFP_KERNEL);
	if (!sor) {
		err = -ENOMEM;
		goto err_allocate;
	}

	res = platform_get_resource_byname(dc->ndev, IORESOURCE_MEM, "sor");
	if (!res) {
		dev_err(&dc->ndev->dev, "sor: no mem resource\n");
		err = -ENOENT;
		goto err_free_sor;
	}

	base_res = request_mem_region(res->start, resource_size(res),
		dc->ndev->name);
	if (!base_res) {
		dev_err(&dc->ndev->dev, "sor: request_mem_region failed\n");
		err = -EBUSY;
		goto err_free_sor;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&dc->ndev->dev, "sor: registers can't be mapped\n");
		err = -ENOENT;
		goto err_release_resource_reg;
	}

	clk = clk_get_sys("sor0", NULL);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&dc->ndev->dev, "sor: can't get clock\n");
		err = -ENOENT;
		goto err_iounmap_reg;
	}

	sor->dc	      = dc;
	sor->base     = base;
	sor->base_res = base_res;
	sor->sor_clk  = clk;
	sor->link_cfg = cfg;
	sor->portnum  = 0;

	tegra_dc_sor_debug_create(sor);

	return sor;


err_iounmap_reg:
	iounmap(base);
err_release_resource_reg:
	release_resource(base_res);
err_free_sor:
	kfree(sor);
err_allocate:
	return ERR_PTR(err);
}

int tegra_dc_sor_set_power_state(struct tegra_dc_sor_data *sor, int pu_pd)
{
	u32 reg_val;
	u32 orig_val;

	orig_val = tegra_sor_readl(sor, NV_SOR_PWR);

	reg_val = pu_pd ? NV_SOR_PWR_NORMAL_STATE_PU :
		NV_SOR_PWR_NORMAL_STATE_PD; /* normal state only */

	if (reg_val == orig_val)
		return 0;	/* No update needed */

	reg_val |= NV_SOR_PWR_SETTING_NEW_TRIGGER;
	tegra_sor_writel(sor, NV_SOR_PWR, reg_val);

	/* Poll to confirm it is done */
	if (tegra_dc_sor_poll_register(sor, NV_SOR_PWR,
			NV_SOR_PWR_SETTING_NEW_DEFAULT_MASK,
			NV_SOR_PWR_SETTING_NEW_DONE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for SOR_PWR = NEW_DONE\n");
		return -EFAULT;
	}
	return 0;
}


void tegra_dc_sor_destroy(struct tegra_dc_sor_data *sor)
{
	clk_put(sor->sor_clk);
	iounmap(sor->base);
	release_resource(sor->base_res);
	kfree(sor);
}

void tegra_dc_sor_set_dp_linkctl(struct tegra_dc_sor_data *sor, bool ena,
	u8 training_pattern, const struct tegra_dc_dp_link_config *cfg)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_DP_LINKCTL(sor->portnum));

	if (ena)
		reg_val |= NV_SOR_DP_LINKCTL_ENABLE_YES;
	else
		reg_val &= NV_SOR_DP_LINKCTL_ENABLE_NO;

	reg_val &= ~NV_SOR_DP_LINKCTL_TUSIZE_MASK;
	reg_val |= (cfg->tu_size << NV_SOR_DP_LINKCTL_TUSIZE_SHIFT);

	if (cfg->enhanced_framing)
		reg_val |= NV_SOR_DP_LINKCTL_ENHANCEDFRAME_ENABLE;

	tegra_sor_writel(sor, NV_SOR_DP_LINKCTL(sor->portnum), reg_val);

	switch (training_pattern) {
	case trainingPattern_1:
		tegra_sor_writel(sor, NV_SOR_DP_TPG, 0x41414141);
		break;
	case trainingPattern_2:
	case trainingPattern_3:
		reg_val = (cfg->link_bw == SOR_LINK_SPEED_G5_4) ?
			0x43434343 : 0x42424242;
		tegra_sor_writel(sor, NV_SOR_DP_TPG, reg_val);
		break;
	default:
		tegra_sor_writel(sor, NV_SOR_DP_TPG, 0x50505050);
		break;
	}
}

static int tegra_dc_sor_enable_lane_sequencer(struct tegra_dc_sor_data *sor,
	bool pu, bool is_lvds)
{
	u32 reg_val;

	/* SOR lane sequencer */
	if (pu)
		reg_val = NV_SOR_LANE_SEQ_CTL_SETTING_NEW_TRIGGER |
			NV_SOR_LANE_SEQ_CTL_SEQUENCE_DOWN |
			NV_SOR_LANE_SEQ_CTL_NEW_POWER_STATE_PU;
	else
		reg_val = NV_SOR_LANE_SEQ_CTL_SETTING_NEW_TRIGGER |
			NV_SOR_LANE_SEQ_CTL_SEQUENCE_UP |
			NV_SOR_LANE_SEQ_CTL_NEW_POWER_STATE_PD;

	if (is_lvds)
		reg_val |= 15 << NV_SOR_LANE_SEQ_CTL_DELAY_SHIFT;

	tegra_sor_writel(sor, NV_SOR_LANE_SEQ_CTL, reg_val);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_LANE_SEQ_CTL,
			NV_SOR_LANE_SEQ_CTL_SETTING_MASK,
			NV_SOR_LANE_SEQ_CTL_SETTING_NEW_DONE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_dbg(&sor->dc->ndev->dev,
			"dp: timeout while waiting for SOR lane sequencer "
			"to power down langes\n");
		return -EFAULT;
	}
	return 0;
}

static int tegra_dc_sor_power_dplanes(struct tegra_dc_sor_data *sor,
	u32 lane_count, bool pu)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_DP_PADCTL(sor->portnum));

	if (pu) {
		switch (lane_count) {
		case 4:
			reg_val |= (NV_SOR_DP_PADCTL_PD_TXD_3_NO |
				NV_SOR_DP_PADCTL_PD_TXD_2_NO);
			/* fall through */
		case 2:
			reg_val |= NV_SOR_DP_PADCTL_PD_TXD_1_NO;
		case 1:
			reg_val |= NV_SOR_DP_PADCTL_PD_TXD_0_NO;
			break;
		default:
			dev_dbg(&sor->dc->ndev->dev,
				"dp: invalid lane number %d\n", lane_count);
			return -EFAULT;
		}

		tegra_sor_writel(sor, NV_SOR_DP_PADCTL(sor->portnum), reg_val);
		tegra_dc_sor_set_lane_count(sor, lane_count);
	}

	return tegra_dc_sor_enable_lane_sequencer(sor, pu, false);
}

void tegra_dc_sor_set_panel_power(struct tegra_dc_sor_data *sor,
	bool power_up)
{
	u32 reg_val;

	/* !!TODO: need to enable panel power through GPIO operations */
	/* Check bug 790854 for HW progress */

	reg_val = tegra_sor_readl(sor, NV_SOR_DP_PADCTL(sor->portnum));

	if (power_up)
		reg_val |= NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERUP;
	else
		reg_val &= ~NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERUP;

	tegra_sor_writel(sor, NV_SOR_DP_PADCTL(sor->portnum), reg_val);
}

static void tegra_dc_sor_termination_cali(struct tegra_dc_sor_data *sor)
{
	u32 termadj;
	u32 cur_try;
	u32 reg_val;

	termadj = cur_try = 0x8;

	tegra_sor_write_field(sor, NV_SOR_PLL1,
		NV_SOR_PLL1_TMDS_TERMADJ_DEFAULT_MASK |
		NV_SOR_PLL1_TMDS_TERM_ENABLE,
		NV_SOR_PLL1_TMDS_TERM_ENABLE |
		termadj << NV_SOR_PLL1_TMDS_TERMADJ_SHIFT);

	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN, /* PDCAL */
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERUP);

	while (cur_try) {
		/* binary search the right value */
		usleep_range(100, 200);
		reg_val = tegra_sor_readl(sor, NV_SOR_PLL1);

		if (reg_val & NV_SOR_PLL1_TERM_COMPOUT_HIGH)
			termadj -= cur_try;
		cur_try >>= 1;
		termadj += cur_try;

		tegra_sor_write_field(sor, NV_SOR_PLL1,
			NV_SOR_PLL1_TMDS_TERMADJ_DEFAULT_MASK,
			termadj << NV_SOR_PLL1_TMDS_TERMADJ_SHIFT);
	}
	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN, /* PDCAL */
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN);
}

static void tegra_dc_sor_config_pwm(struct tegra_dc_sor_data *sor, u32 pwm_div,
	u32 pwm_dutycycle)
{
	tegra_sor_writel(sor, NV_SOR_PWM_DIV, pwm_div);
	tegra_sor_writel(sor, NV_SOR_PWM_CTL,
		(pwm_dutycycle & NV_SOR_PWM_CTL_DUTY_CYCLE_MASK) |
		NV_SOR_PWM_CTL_SETTING_NEW_TRIGGER);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_PWM_CTL,
			NV_SOR_PWM_CTL_SETTING_NEW_SHIFT,
			NV_SOR_PWM_CTL_SETTING_NEW_DONE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_dbg(&sor->dc->ndev->dev,
			"dp: timeout while waiting for SOR PWM setting\n");
	}
}

static void tegra_dc_sor_set_dp_mode(struct tegra_dc_sor_data *sor,
	const struct tegra_dc_dp_link_config *cfg)
{
	u32 reg_val;

	BUG_ON(!cfg || !cfg->is_valid);

	tegra_dc_sor_set_link_bandwidth(sor, cfg->link_bw);

	tegra_dc_sor_set_dp_linkctl(sor, true, trainingPattern_None, cfg);
	reg_val = tegra_sor_readl(sor, NV_SOR_DP_CONFIG(sor->portnum));
	reg_val &= ~NV_SOR_DP_CONFIG_WATERMARK_MASK;
	reg_val |= cfg->watermark;
	reg_val &= ~NV_SOR_DP_CONFIG_ACTIVESYM_COUNT_MASK;
	reg_val |= (cfg->active_count <<
		NV_SOR_DP_CONFIG_ACTIVESYM_COUNT_SHIFT);
	reg_val &= ~NV_SOR_DP_CONFIG_ACTIVESYM_FRAC_MASK;
	reg_val |= (cfg->active_frac <<
		NV_SOR_DP_CONFIG_ACTIVESYM_FRAC_SHIFT);
	if (cfg->activepolarity)
		reg_val |= NV_SOR_DP_CONFIG_ACTIVESYM_POLARITY_POSITIVE;
	else
		reg_val &= ~NV_SOR_DP_CONFIG_ACTIVESYM_POLARITY_POSITIVE;
	reg_val |= (NV_SOR_DP_CONFIG_ACTIVESYM_CNTL_ENABLE |
		NV_SOR_DP_CONFIG_RD_RESET_VAL_NEGATIVE);

	tegra_sor_writel(sor, NV_SOR_DP_CONFIG(sor->portnum), reg_val);

	/* program h/vblank sym */
	tegra_sor_write_field(sor, NV_SOR_DP_AUDIO_HBLANK_SYMBOLS,
		NV_SOR_DP_AUDIO_HBLANK_SYMBOLS_MASK, cfg->hblank_sym);

	tegra_sor_write_field(sor, NV_SOR_DP_AUDIO_VBLANK_SYMBOLS,
		NV_SOR_DP_AUDIO_VBLANK_SYMBOLS_MASK, cfg->vblank_sym);
}

static inline void tegra_dc_sor_super_update(struct tegra_dc_sor_data *sor)
{
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE0, 0);
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE0, 1);
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE0, 0);
}

static inline void tegra_dc_sor_update(struct tegra_dc_sor_data *sor)
{
	tegra_sor_writel(sor, NV_SOR_STATE0, 0);
	tegra_sor_writel(sor, NV_SOR_STATE0, 1);
	tegra_sor_writel(sor, NV_SOR_STATE0, 0);
}



static void tegra_dc_sor_io_set_dpd(struct tegra_dc_sor_data *sor, bool up)
{
	u32 reg_val;
	static void __iomem *pmc_base = IO_ADDRESS(TEGRA_PMC_BASE);
	unsigned long timeout_jf;

	if (up) {
		writel(APBDEV_PMC_DPD_SAMPLE_ON_ENABLE,
			pmc_base + APBDEV_PMC_DPD_SAMPLE);
		writel(10, pmc_base + APBDEV_PMC_SEL_DPD_TIM);
	}

	reg_val = readl(pmc_base + APBDEV_PMC_IO_DPD2_REQ);
	reg_val &= ~(APBDEV_PMC_IO_DPD2_REQ_LVDS_ON ||
		APBDEV_PMC_IO_DPD2_REQ_CODE_DEFAULT_MASK);

	reg_val = up ? APBDEV_PMC_IO_DPD2_REQ_LVDS_ON |
		APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_OFF :
		APBDEV_PMC_IO_DPD2_REQ_LVDS_OFF |
		APBDEV_PMC_IO_DPD2_REQ_CODE_DPD_ON;

	writel(reg_val, pmc_base + APBDEV_PMC_IO_DPD2_REQ);

	/* Polling */
	timeout_jf = jiffies + msecs_to_jiffies(10);
	do {
		usleep_range(20, 40);
		reg_val = readl(pmc_base + APBDEV_PMC_IO_DPD2_STATUS);
	} while (((reg_val & APBDEV_PMC_IO_DPD2_STATUS_LVDS_ON) != 0) &&
		time_after(timeout_jf, jiffies));

	if ((reg_val & APBDEV_PMC_IO_DPD2_STATUS_LVDS_ON) != 0)
		dev_err(&sor->dc->ndev->dev,
			"PMC_IO_DPD2 polling failed (0x%x)\n", reg_val);

	if (up)
		writel(APBDEV_PMC_DPD_SAMPLE_ON_DISABLE,
			pmc_base + APBDEV_PMC_DPD_SAMPLE);
}


/* The SOR power sequencer does not work for t124 so SW has to
   go through the power sequence manually */
/* Power up steps from spec: */
/* STEP	PDPORT	PDPLL	PDBG	PLLVCOD	PLLCAPD	E_DPD	PDCAL */
/* 1	1	1	1	1	1	1	1 */
/* 2	1	1	1	1	1	0	1 */
/* 3	1	1	0	1	1	0	1 */
/* 4	1	0	0	0	0	0	1 */
/* 5	0	0	0	0	0	0	1 */
static void tegra_dc_sor_power_up(struct tegra_dc_sor_data *sor,
				  bool is_lvds)
{
	if (sor->power_is_up)
		return;

	/* Enable safe clock */
	tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 0);
	tegra_dc_sor_set_link_bandwidth(sor,
		is_lvds ? NV_SOR_CLK_CNTRL_DP_LINK_SPEED_LVDS :
		NV_SOR_CLK_CNTRL_DP_LINK_SPEED_G1_62);

	/* step 1 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK | /* PDPORT */
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK | /* PDBG */
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK, /* PLLCAPD */
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_ENABLE |
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_ENABLE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_ENABLE);
	tegra_sor_write_field(sor, NV_SOR_PLL0,
		NV_SOR_PLL0_PWR_MASK | /* PDPLL */
		NV_SOR_PLL0_VCOPD_MASK, /* PLLVCOPD */
		NV_SOR_PLL0_PWR_OFF |
		NV_SOR_PLL0_VCOPD_ASSERT);
	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN, /* PDCAL */
		NV_SOR_DP_PADCTL_PAD_CAL_PD_POWERDOWN);

	/* step 2 */
	tegra_dc_sor_io_set_dpd(sor, true);
	usleep_range(5, 100);	/* sleep > 5us */

	/* step 3 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_DISABLE);
	usleep_range(20, 100);	/* sleep > 20 us */

	/* step 4 */
	tegra_sor_write_field(sor, NV_SOR_PLL0,
		NV_SOR_PLL0_PWR_MASK | /* PDPLL */
		NV_SOR_PLL0_VCOPD_MASK, /* PLLVCOPD */
		NV_SOR_PLL0_PWR_ON | NV_SOR_PLL0_VCOPD_RESCIND);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK, /* PLLCAPD */
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE);
	usleep_range(200, 1000);

	/* step 5 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK, /* PDPORT */
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_DISABLE);

	sor->power_is_up = true;
}

/* Powerdown steps from the spec: */
/* STEP	PDPORT	PDPLL	PDBG	PLLVCOD	PLLCAPD	E_DPD	PDCAL */
/* 1	0	0	0	0	0	0	1 */
/* 2	1	0	0	0	0	0	1 */
/* 3	1	1	0	1	1	0	1 */
/* 4	1	1	1	1	1	0	1 */
/* 5	1	1	1	1	1	1	1 */
static void tegra_dc_sor_power_down(struct tegra_dc_sor_data *sor)
{
	if (!sor->power_is_up)
		return;

	/* use safe clock */
	tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 0);

	/* step 1 -- not necessary */

	/* step 2 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK, /* PDPORT */
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_ENABLE);
	udelay(5);	/* sleep > 5us */

	/* step 3 */
	tegra_sor_write_field(sor, NV_SOR_PLL0,
		NV_SOR_PLL0_PWR_MASK | /* PDPLL */
		NV_SOR_PLL0_VCOPD_MASK, /* PLLVCOPD */
		NV_SOR_PLL0_PWR_OFF | NV_SOR_PLL0_VCOPD_ASSERT);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK, /* PLLCAPD */
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_ENABLE);
	udelay(5);	/* sleep > 5us */

	/* step 4 */
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_ENABLE);
	udelay(5);

	/* step 5 */
	tegra_dc_sor_io_set_dpd(sor, false);

	sor->power_is_up = false;
}


static void tegra_dc_sor_config_panel(struct tegra_dc_sor_data *sor,
	bool is_lvds)
{
	const struct tegra_dc_out_pin	*pins	  = sor->dc->out->out_pins;
	const struct tegra_dc_mode	*dc_mode  = &sor->dc->mode;

	const int	head_num = sor->dc->ndev->id;
	u32		reg_val	 = NV_SOR_STATE1_ASY_OWNER_HEAD0 << head_num;
	u32		vtotal, htotal;
	u32		vsync_end, hsync_end;
	u32		vblank_end, hblank_end;
	u32		vblank_start, hblank_start;
	int		i;

	reg_val |= is_lvds ? NV_SOR_STATE1_ASY_PROTOCOL_LVDS_CUSTOM :
		NV_SOR_STATE1_ASY_PROTOCOL_DP_A;
	reg_val |= NV_SOR_STATE1_ASY_SUBOWNER_NONE |
		NV_SOR_STATE1_ASY_CRCMODE_COMPLETE_RASTER;

	for (i = 0; pins && (i < sor->dc->out->n_out_pins); i++) {
		switch (pins[i].name) {
		case TEGRA_DC_OUT_PIN_DATA_ENABLE:
			if (pins[i].pol == TEGRA_DC_OUT_PIN_POL_LOW)
				reg_val |=
				NV_SOR_STATE1_ASY_DEPOL_NEGATIVE_TRUE;
			break;
		case TEGRA_DC_OUT_PIN_H_SYNC:
			if (pins[i].pol == TEGRA_DC_OUT_PIN_POL_LOW)
				reg_val |=
				NV_SOR_STATE1_ASY_HSYNCPOL_NEGATIVE_TRUE;
			break;
		case TEGRA_DC_OUT_PIN_V_SYNC:
			if (pins[i].pol == TEGRA_DC_OUT_PIN_POL_LOW)
				reg_val |=
				NV_SOR_STATE1_ASY_VSYNCPOL_NEGATIVE_TRUE;
			break;
		default:	/* Ignore other pin setting */
			break;
		}
	}

	reg_val |= (sor->dc->pdata->default_out->depth > 18) ?
		NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_24_444 :
		NV_SOR_STATE1_ASY_PIXELDEPTH_BPP_18_444;

	tegra_sor_writel(sor, NV_SOR_STATE1, reg_val);

	/* Skipping programming NV_HEAD_STATE0, assuming:
	   interlacing: PROGRESSIVE, dynamic range: VESA, colorspace: RGB */

	BUG_ON(!dc_mode);
	vtotal = dc_mode->v_sync_width + dc_mode->v_back_porch +
		dc_mode->v_active + dc_mode->v_front_porch;
	htotal = dc_mode->h_sync_width + dc_mode->h_back_porch +
		dc_mode->h_active + dc_mode->h_front_porch;
	tegra_sor_writel(sor, NV_HEAD_STATE1(head_num),
		vtotal << NV_HEAD_STATE1_VTOTAL_SHIFT |
		htotal << NV_HEAD_STATE1_HTOTAL_SHIFT);

	vsync_end = dc_mode->v_sync_width - 1;
	hsync_end = dc_mode->h_sync_width - 1;
	tegra_sor_writel(sor, NV_HEAD_STATE2(head_num),
		vsync_end << NV_HEAD_STATE2_VSYNC_END_SHIFT |
		hsync_end << NV_HEAD_STATE2_HSYNC_END_SHIFT);

	vblank_end = vsync_end + dc_mode->v_back_porch;
	hblank_end = hsync_end + dc_mode->h_back_porch;
	tegra_sor_writel(sor, NV_HEAD_STATE3(head_num),
		vblank_end << NV_HEAD_STATE3_VBLANK_END_SHIFT |
		hblank_end << NV_HEAD_STATE3_HBLANK_END_SHIFT);

	vblank_start = vblank_end + dc_mode->v_active;
	hblank_start = hblank_end + dc_mode->h_active;
	tegra_sor_writel(sor, NV_HEAD_STATE4(head_num),
		vblank_start << NV_HEAD_STATE4_VBLANK_START_SHIFT |
		hblank_start << NV_HEAD_STATE4_HBLANK_START_SHIFT);

	/* TODO: adding interlace mode support */
	tegra_sor_writel(sor, NV_HEAD_STATE5(head_num), 0x1);

	tegra_sor_write_field(sor, NV_SOR_CSTM,
		NV_SOR_CSTM_ROTCLK_DEFAULT_MASK |
		NV_SOR_CSTM_LVDS_EN_ENABLE,
		2 << NV_SOR_CSTM_ROTCLK_SHIFT |
		is_lvds ? NV_SOR_CSTM_LVDS_EN_ENABLE :
		NV_SOR_CSTM_LVDS_EN_DISABLE);

	tegra_dc_sor_config_pwm(sor, 1024, 1024);
}

static void tegra_dc_sor_enable_clk(struct tegra_dc_sor_data *sor)
{
	if (!tegra_is_clk_enabled(sor->sor_clk))
		clk_prepare_enable(sor->sor_clk);
}

static void tegra_dc_sor_disable_clk(struct tegra_dc_sor_data *sor)
{
	if (tegra_is_clk_enabled(sor->sor_clk))
		clk_disable_unprepare(sor->sor_clk);
}

static void tegra_dc_sor_enable_dc(struct tegra_dc_sor_data *sor)
{
	struct tegra_dc		*dc   = sor->dc;
	u32	reg_val = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);

	tegra_dc_writel(dc, reg_val | WRITE_MUX_ACTIVE, DC_CMD_STATE_ACCESS);

	if (tegra_platform_is_fpga()) {
		tegra_dc_writel(dc, 0, DC_DISP_DISP_CLOCK_CONTROL);
		tegra_dc_writel(dc, 0xe, DC_DISP_DC_MCCIF_FIFOCTRL);
	}

	tegra_dc_writel(dc, VSYNC_H_POSITION(1), DC_DISP_DISP_TIMING_OPTIONS);

	/* Enable DC */
	tegra_dc_writel(dc, DISP_CTRL_MODE_C_DISPLAY, DC_CMD_DISPLAY_COMMAND);
	tegra_dc_writel(dc, reg_val, DC_CMD_STATE_ACCESS);
}

static void tegra_dc_sor_attach_lvds(struct tegra_dc_sor_data *sor)
{
	/* Set head owner */
	tegra_sor_write_field(sor, NV_SOR_STATE1,
		NV_SOR_STATE1_ASY_SUBOWNER_DEFAULT_MASK,
		NV_SOR_STATE1_ASY_SUBOWNER_BOTH);

	tegra_dc_sor_update(sor);

	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_SAFE |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
			NV_SOR_TEST_ATTACHED_DEFAULT_MASK,
			NV_SOR_TEST_ATTACHED_TRUE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for ATTACHED = TRUE\n");
		return;
	}

	/* OR mode: normal */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_SLEEP |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

	/* then awake */
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1,
		NV_SOR_SUPER_STATE1_ASY_HEAD_OP_AWAKE |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_YES);
	tegra_dc_sor_super_update(sor);

}

void tegra_dc_sor_enable_dp(struct tegra_dc_sor_data *sor)
{
	const struct tegra_dc_dp_link_config *cfg = sor->link_cfg;

	tegra_dc_sor_enable_clk(sor);

	tegra_sor_write_field(sor, NV_SOR_CLK_CNTRL,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_MASK,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_SINGLE_DPCLK);

	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX6_BANDGAP_POWERDOWN_DISABLE);
	usleep_range(20, 100);

	tegra_sor_write_field(sor, NV_SOR_PLL3,
		NV_SOR_PLL3_PLLVDD_MODE_MASK,
		NV_SOR_PLL3_PLLVDD_MODE_V3_3);
	tegra_sor_writel(sor, NV_SOR_PLL0,
		0x1 << NV_SOR_PLL0_ICHPMP_SHFIT |
		0x3 << NV_SOR_PLL0_VCOCAP_SHIFT |
		NV_SOR_PLL0_PLLREG_LEVEL_V45 |
		NV_SOR_PLL0_RESISTORSEL_EXT |
		NV_SOR_PLL0_PWR_ON | NV_SOR_PLL0_VCOPD_RESCIND);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX1_SEQ_MASK | NV_SOR_PLL2_AUX9_LVDSEN_OVERRIDE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
		NV_SOR_PLL2_AUX1_SEQ_PLLCAPPD_OVERRIDE |
		NV_SOR_PLL2_AUX9_LVDSEN_OVERRIDE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE);
	tegra_sor_writel(sor, NV_SOR_PLL1,
		NV_SOR_PLL1_TERM_COMPOUT_HIGH | NV_SOR_PLL1_TMDS_TERM_ENABLE |
		0x0 << NV_SOR_PLL1_LVDSCM_SHIFT);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_PLL2,
			NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
			NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE,
			100, TEGRA_SOR_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev, "DP failed to lock PLL\n");
		return;
	}

	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX2_MASK | NV_SOR_PLL2_AUX7_PORT_POWERDOWN_MASK,
		NV_SOR_PLL2_AUX2_OVERRIDE_POWERDOWN |
		NV_SOR_PLL2_AUX7_PORT_POWERDOWN_DISABLE);

	tegra_dc_sor_power_up(sor, false);
	tegra_dc_sor_termination_cali(sor);

	/* re-enable SOR clock */
	tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 3);

	/* Power up lanes */
	BUG_ON(!cfg);
	tegra_dc_sor_power_dplanes(sor, cfg->lane_count, true);
	tegra_dc_sor_set_dp_mode(sor, cfg);
}


void tegra_dc_sor_attach(struct tegra_dc_sor_data *sor)
{
	u32 reg_val;

	tegra_dc_sor_enable_dc(sor);
	tegra_dc_sor_config_panel(sor, false);

	tegra_dc_writel(sor->dc, 0x9f00, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(sor->dc, 0x9f, DC_CMD_STATE_CONTROL);

	tegra_dc_writel(sor->dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE |
		PW3_ENABLE | PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
		DC_CMD_DISPLAY_POWER_CONTROL);

	tegra_dc_writel(sor->dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(sor->dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_writel(sor->dc, SOR_ENABLE, DC_DISP_DISP_WIN_OPTIONS);

	/* Attach head */
	tegra_dc_sor_update(sor);
	reg_val = NV_SOR_SUPER_STATE1_ASY_HEAD_OP_AWAKE |
		NV_SOR_SUPER_STATE1_ASY_ORMODE_NORMAL |
		NV_SOR_SUPER_STATE1_ATTACHED_NO;
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1, reg_val);
	tegra_dc_sor_super_update(sor);

	reg_val |= NV_SOR_SUPER_STATE1_ATTACHED_YES;
	tegra_sor_writel(sor, NV_SOR_SUPER_STATE1, reg_val);
	tegra_dc_sor_super_update(sor);

	if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
			NV_SOR_TEST_ATTACHED_DEFAULT_MASK,
			NV_SOR_TEST_ATTACHED_TRUE,
			100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for ATTACHED = TRUE\n");
	}

	if (tegra_dc_sor_poll_register(sor, NV_SOR_TEST,
			NV_SOR_TEST_ACT_HEAD_OPMODE_DEFAULT_MASK,
			NV_SOR_TEST_ACT_HEAD_OPMODE_AWAKE,
			100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for OPMOD = AWAKE\n");
	}
}

void tegra_dc_sor_enable_lvds(struct tegra_dc_sor_data *sor,
	bool balanced, bool conforming)
{
	u32 reg_val;

	tegra_dc_sor_enable_dc(sor);
	tegra_dc_sor_config_panel(sor, true);
	tegra_dc_writel(sor->dc, 0x9f00, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(sor->dc, 0x9f, DC_CMD_STATE_CONTROL);

	tegra_dc_writel(sor->dc, PW0_ENABLE | PW1_ENABLE | PW2_ENABLE |
		PW3_ENABLE | PW4_ENABLE | PM0_ENABLE | PM1_ENABLE,
		DC_CMD_DISPLAY_POWER_CONTROL);

	tegra_dc_writel(sor->dc, SOR_ENABLE, DC_DISP_DISP_WIN_OPTIONS);

	tegra_sor_write_field(sor, NV_SOR_PLL3,
		NV_SOR_PLL3_PLLVDD_MODE_MASK,
		NV_SOR_PLL3_PLLVDD_MODE_V1_8);

	tegra_sor_writel(sor, NV_SOR_PLL1,
		NV_SOR_PLL1_TERM_COMPOUT_HIGH | NV_SOR_PLL1_TMDS_TERM_ENABLE);
	tegra_sor_write_field(sor, NV_SOR_PLL2,
		NV_SOR_PLL2_AUX1_SEQ_MASK |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_MASK,
		NV_SOR_PLL2_AUX1_SEQ_PLLCAPPD_OVERRIDE |
		NV_SOR_PLL2_AUX8_SEQ_PLLCAPPD_ENFORCE_DISABLE);


	reg_val = NV_SOR_LVDS_LINKACTB_DISABLE |
		NV_SOR_LVDS_LINKACTA_ENABLE |
		NV_SOR_LVDS_UPPER_TRUE |
		NV_SOR_LVDS_PD_TXCB_DISABLE |
		NV_SOR_LVDS_PD_TXDB_3_DISABLE |
		NV_SOR_LVDS_PD_TXDB_2_DISABLE |
		NV_SOR_LVDS_PD_TXDB_1_DISABLE |
		NV_SOR_LVDS_PD_TXDB_0_DISABLE |
		NV_SOR_LVDS_PD_TXDA_2_ENABLE |
		NV_SOR_LVDS_PD_TXDA_1_ENABLE |
		NV_SOR_LVDS_PD_TXDA_0_ENABLE;
	if (!conforming && (sor->dc->pdata->default_out->depth == 18))
		reg_val |= (NV_SOR_LVDS_PD_TXDA_3_DISABLE);

	tegra_sor_writel(sor, NV_SOR_LVDS, reg_val);
	tegra_sor_writel(sor, NV_SOR_LANE_DRIVE_CURRENT(sor->portnum),
		0x40404040);

#if 0
	tegra_sor_write_field(sor, NV_SOR_LVDS,
		NV_SOR_LVDS_BALANCED_DEFAULT_MASK,
		balanced ? NV_SOR_LVDS_BALANCED_ENABLE :
		NV_SOR_LVDS_BALANCED_DISABLE);
	tegra_sor_write_field(sor, NV_SOR_LVDS,
		NV_SOR_LVDS_ROTDAT_DEFAULT_MASK,
		conforming ? 6 << NV_SOR_LVDS_ROTDAT_SHIFT :
		0 << NV_SOR_LVDS_ROTDAT_SHIFT);
#endif

	tegra_dc_sor_power_up(sor, true);

	tegra_sor_writel(sor, NV_SOR_SEQ_INST(0),
		NV_SOR_SEQ_INST_LANE_SEQ_RUN |
		NV_SOR_SEQ_INST_HALT_TRUE);
	tegra_sor_writel(sor, NV_SOR_DP_SPARE(sor->portnum),
		NV_SOR_DP_SPARE_SEQ_ENABLE_YES |
		NV_SOR_DP_SPARE_PANEL_INTERNAL |
		NV_SOR_DP_SPARE_SOR_CLK_SEL_MACRO_SORCLK);

	tegra_dc_sor_enable_lane_sequencer(sor, true, true);
	tegra_dc_sor_set_link_bandwidth(sor, SOR_LINK_SPEED_LVDS);

	tegra_dc_sor_attach_lvds(sor);

	tegra_sor_writel(sor, NV_SOR_CLK_CNTRL,
		NV_SOR_CLK_CNTRL_DP_CLK_SEL_SINGLE_PCLK |
		NV_SOR_CLK_CNTRL_DP_LINK_SPEED_LVDS);




	/* re-enable SOR clock */
	tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 1);

	if ((tegra_dc_sor_set_power_state(sor, 1))) {
		dev_err(&sor->dc->ndev->dev,
			"Failed to power up SOR sequencer for LVDS\n");
		return;
	}

	if (tegra_dc_sor_poll_register(sor, NV_SOR_PWR,
			NV_SOR_PWR_MODE_DEFAULT_MASK,
			NV_SOR_PWR_MODE_NORMAL,
			100, TEGRA_SOR_ATTACH_TIMEOUT_MS)) {
		dev_err(&sor->dc->ndev->dev,
			"dc timeout waiting for ATTACHED = TRUE\n");
		return;
	}
}

void tegra_dc_sor_disable(struct tegra_dc_sor_data *sor, bool is_lvds)
{
	struct tegra_dc *dc = sor->dc;

	tegra_dc_sor_power_down(sor);

	/* Power down the SOR sequencer */
	if (tegra_dc_sor_set_power_state(sor, 0)) {
		dev_err(&dc->ndev->dev,
			"Failed to power down SOR sequencer\n");
		return;
	}

	/* Power down DP lanes */
	if (!is_lvds && tegra_dc_sor_power_dplanes(sor, 4, false)) {
		dev_err(&dc->ndev->dev,
			"Failed to power down dp lanes\n");
		return;
	}

	tegra_dc_writel(dc, 0, DC_DISP_DISP_WIN_OPTIONS);
	tegra_dc_writel(dc, GENERAL_ACT_REQ << 8, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_sor_disable_clk(sor);
	/* Reset SOR clk */
	tegra_periph_reset_assert(sor->sor_clk);
}

int tegra_dc_sor_set_dp_packet(struct tegra_dc_sor_data *sor,
	u8 *packet)
{
	/* No need to set the infoframe yet as there is no audio or
	   stereo support. This is a placeholder for now */
	return 0;
}


void tegra_dc_sor_set_internal_panel(struct tegra_dc_sor_data *sor, bool is_int)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_DP_SPARE(sor->portnum));
	if (is_int)
		reg_val |= NV_SOR_DP_SPARE_PANEL_INTERNAL;
	else
		reg_val &= ~NV_SOR_DP_SPARE_PANEL_INTERNAL;

	reg_val |= NV_SOR_DP_SPARE_SOR_CLK_SEL_MACRO_SORCLK |
		NV_SOR_DP_SPARE_SEQ_ENABLE_YES;
	tegra_sor_writel(sor, NV_SOR_DP_SPARE(sor->portnum), reg_val);
}

void tegra_dc_sor_read_link_config(struct tegra_dc_sor_data *sor, u8 *link_bw,
				   u8 *lane_count)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_CLK_CNTRL);
	*link_bw = (reg_val & NV_SOR_CLK_CNTRL_DP_LINK_SPEED_MASK)
		>> NV_SOR_CLK_CNTRL_DP_LINK_SPEED_SHIFT;
	reg_val = tegra_sor_readl(sor,
		NV_SOR_DP_LINKCTL(sor->portnum));

	switch (reg_val & NV_SOR_DP_LINKCTL_LANECOUNT_MASK) {
	case NV_SOR_DP_LINKCTL_LANECOUNT_ZERO:
		*lane_count = 0;
		break;
	case NV_SOR_DP_LINKCTL_LANECOUNT_ONE:
		*lane_count = 1;
		break;
	case NV_SOR_DP_LINKCTL_LANECOUNT_TWO:
		*lane_count = 2;
		break;
	case NV_SOR_DP_LINKCTL_LANECOUNT_FOUR:
		*lane_count = 4;
		break;
	default:
		dev_err(&sor->dc->ndev->dev, "Unknown lane count\n");
	}
}

void tegra_dc_sor_set_link_bandwidth(struct tegra_dc_sor_data *sor, u8 link_bw)
{
	tegra_sor_write_field(sor, NV_SOR_CLK_CNTRL,
		NV_SOR_CLK_CNTRL_DP_LINK_SPEED_MASK,
		link_bw << NV_SOR_CLK_CNTRL_DP_LINK_SPEED_SHIFT);
}

void tegra_dc_sor_set_lane_count(struct tegra_dc_sor_data *sor, u8 lane_count)
{
	u32 reg_val;

	reg_val = tegra_sor_readl(sor, NV_SOR_DP_LINKCTL(sor->portnum));
	reg_val &= ~NV_SOR_DP_LINKCTL_LANECOUNT_MASK;
	switch (lane_count) {
	case 0:
		break;
	case 1:
		reg_val |= NV_SOR_DP_LINKCTL_LANECOUNT_ONE;
		break;
	case 2:
		reg_val |= NV_SOR_DP_LINKCTL_LANECOUNT_TWO;
		break;
	case 4:
		reg_val |= NV_SOR_DP_LINKCTL_LANECOUNT_FOUR;
		break;
	default:
		/* 0 should be handled earlier. */
		dev_err(&sor->dc->ndev->dev, "dp: Invalid lane count %d\n",
			lane_count);
		return;
	}
	tegra_sor_writel(sor, NV_SOR_DP_LINKCTL(sor->portnum), reg_val);
}

void tegra_dc_sor_setup_clk(struct tegra_dc_sor_data *sor, struct clk *clk,
	bool is_lvds)
{
	struct clk	*parent_clk;

	tegra_dc_sor_enable_clk(sor);

	parent_clk = clk_get_parent(clk);
	BUG_ON(!parent_clk);

	if (sor->dc->mode.pclk != clk_get_rate(parent_clk))
		clk_set_rate(parent_clk, sor->dc->mode.pclk);

	/* Only enable safe clock initially */
	tegra_clk_cfg_ex(sor->sor_clk, TEGRA_CLK_SOR_CLK_SEL, 0);
}

void tegra_sor_precharge_lanes(struct tegra_dc_sor_data *sor)
{
	const struct tegra_dc_dp_link_config *cfg = sor->link_cfg;
	u32 val = 0;

	switch (cfg->lane_count) {
	case 4:
		val |= (NV_SOR_DP_PADCTL_PD_TXD_3_NO |
			NV_SOR_DP_PADCTL_PD_TXD_2_NO);
		/* fall through */
	case 2:
		val |= NV_SOR_DP_PADCTL_PD_TXD_1_NO;
	case 1:
		val |= NV_SOR_DP_PADCTL_PD_TXD_0_NO;
		break;
	default:
		dev_dbg(&sor->dc->ndev->dev,
			"dp: invalid lane number %d\n", cfg->lane_count);
		return;
	}

	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		(0xf << NV_SOR_DP_PADCTL_COMODE_TXD_0_DP_TXD_2_SHIFT),
		(val << NV_SOR_DP_PADCTL_COMODE_TXD_0_DP_TXD_2_SHIFT));
	usleep_range(15, 100);
	tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
		(0xf << NV_SOR_DP_PADCTL_COMODE_TXD_0_DP_TXD_2_SHIFT), 0);
}

void tegra_dc_sor_set_lane_parm(struct tegra_dc_sor_data *sor,
	const struct tegra_dc_dp_link_config *cfg)
{
	int idx = lt_param_idx(cfg->link_bw);
	struct tegra_dc_dp_lt_settings *lt_setting = NULL;
	struct tegra_dp_out *dp_data = sor->dc->pdata->default_out->dp_out;

	if (dp_data && dp_data->lt_settings) {
		/* If panel only provides one entry, use it anyway */
		if (idx >= dp_data->n_lt_settings)
			idx = dp_data->n_lt_settings - 1;
		lt_setting = &dp_data->lt_settings[idx];
	}

	BUG_ON(!lt_setting && !cfg->vs_pe_valid);
	tegra_sor_writel(sor, NV_SOR_LANE_DRIVE_CURRENT(sor->portnum),
		cfg->vs_pe_valid ? cfg->drive_current :
		lt_setting->drive_current);
	tegra_sor_writel(sor, NV_SOR_PR(sor->portnum),
		cfg->vs_pe_valid ? cfg->preemphasis :
		lt_setting->lane_preemphasis);
	tegra_sor_writel(sor, NV_SOR_POSTCURSOR(sor->portnum),
		cfg->vs_pe_valid ? cfg->postcursor :
		lt_setting->post_cursor);

	tegra_sor_writel(sor, NV_SOR_LVDS, 0);
	tegra_dc_sor_set_link_bandwidth(sor, cfg->link_bw);
	tegra_dc_sor_set_lane_count(sor, cfg->lane_count);

	if (cfg->vs_pe_valid) {
		/* todo: Full link training need to save/restore
		   tx_pu and load_adj values */
	} else {
		tegra_sor_write_field(sor, NV_SOR_DP_PADCTL(sor->portnum),
			NV_SOR_DP_PADCTL_TX_PU_ENABLE |
			NV_SOR_DP_PADCTL_TX_PU_VALUE_DEFAULT_MASK,
			NV_SOR_DP_PADCTL_TX_PU_ENABLE |
			lt_setting->tx_pu
			<< NV_SOR_DP_PADCTL_TX_PU_VALUE_SHIFT);

		tegra_sor_write_field(sor, NV_SOR_PLL1,
			NV_SOR_PLL1_LOADADJ_DEFAULT_MASK,
			lt_setting->load_adj << NV_SOR_PLL1_LOADADJ_SHIFT);
	}

	/* Precharge */
	tegra_sor_precharge_lanes(sor);
}

void tegra_dc_sor_modeset_notifier(struct tegra_dc_sor_data *sor,
	bool is_lvds)
{
	tegra_dc_sor_config_panel(sor, is_lvds);
	tegra_dc_sor_update(sor);
	tegra_dc_sor_super_update(sor);
}

