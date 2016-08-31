/*
 * drivers/video/tegra/dc/dp.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION, All rights reserved.
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


#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/tegra-soc.h>

#include <mach/dc.h>
#include <mach/fb.h>

#include "dp.h"
#include "sor.h"
#include "sor_regs.h"
#include "dpaux_regs.h"
#include "dc_priv.h"

static int tegra_dp_lt(struct tegra_dc_dp_data *dp);

static inline u32 tegra_dpaux_readl(struct tegra_dc_dp_data *dp, u32 reg)
{
	return readl(dp->aux_base + reg * 4);
}

static inline void tegra_dpaux_writel(struct tegra_dc_dp_data *dp,
	u32 reg, u32 val)
{
	writel(val, dp->aux_base + reg * 4);
}

static inline void tegra_dp_int_en(struct tegra_dc_dp_data *dp, u32 intr)
{
	u32 val;

	/* clear pending interrupt */
	tegra_dpaux_writel(dp, DPAUX_INTR_AUX, intr);

	val = tegra_dpaux_readl(dp, DPAUX_INTR_EN_AUX);
	val |= intr;

	tegra_dpaux_writel(dp, DPAUX_INTR_EN_AUX, val);
}

static inline void tegra_dp_int_dis(struct tegra_dc_dp_data *dp, u32 intr)
{
	u32 val;

	val = tegra_dpaux_readl(dp, DPAUX_INTR_EN_AUX);
	val &= intr;

	tegra_dpaux_writel(dp, DPAUX_INTR_EN_AUX, val);
}

static inline void tegra_dp_enable_irq(u32 irq)
{
	if (tegra_platform_is_fpga())
		return;

	enable_irq(irq);
}

static inline void tegra_dp_disable_irq(u32 irq)
{
	if (tegra_platform_is_fpga())
		return;

	disable_irq(irq);
}

static inline u32 tegra_dc_dpaux_poll_register(struct tegra_dc_dp_data *dp,
	u32 reg, u32 mask, u32 exp_val, u32 poll_interval_us, u32 timeout_ms)
{
	unsigned long	timeout_jf = jiffies + msecs_to_jiffies(timeout_ms);
	u32		reg_val	   = 0;

	do {
		usleep_range(poll_interval_us, poll_interval_us << 1);
		reg_val = tegra_dpaux_readl(dp, reg);
	} while (((reg_val & mask) != exp_val) &&
		time_after(timeout_jf, jiffies));

	if ((reg_val & mask) == exp_val)
		return 0;	/* success */
	dev_dbg(&dp->dc->ndev->dev,
		"dpaux_poll_register 0x%x: timeout\n", reg);
	return jiffies - timeout_jf + 1;
}


static inline int tegra_dpaux_wait_transaction(struct tegra_dc_dp_data *dp)
{
	/* According to DP spec, each aux transaction needs to finish
	   within 40ms. */
	if (tegra_dc_dpaux_poll_register(dp, DPAUX_DP_AUXCTL,
			DPAUX_DP_AUXCTL_TRANSACTREQ_MASK,
			DPAUX_DP_AUXCTL_TRANSACTREQ_DONE,
			100, DP_AUX_TIMEOUT_MS) != 0) {
		dev_err(&dp->dc->ndev->dev,
			"dp: DPAUX transaction timeout\n");
		return -EFAULT;
	}
	return 0;
}


static int tegra_dc_dpaux_write_chunk(struct tegra_dc_dp_data *dp, u32 cmd,
	u32 addr, u8 *data, u32 *size, u32 *aux_stat)
{
	int	i;
	u32	reg_val;
	u32	timeout_retries = DP_AUX_TIMEOUT_MAX_TRIES;
	u32	defer_retries	= DP_AUX_DEFER_MAX_TRIES;

	if (*size >= DP_AUX_MAX_BYTES)
		return -EINVAL;	/* only write one chunk of data */

	/* Make sure the command is write command */
	switch (cmd) {
	case DPAUX_DP_AUXCTL_CMD_I2CWR:
	case DPAUX_DP_AUXCTL_CMD_MOTWR:
	case DPAUX_DP_AUXCTL_CMD_AUXWR:
		break;
	default:
		dev_err(&dp->dc->ndev->dev, "dp: aux write cmd 0x%x is invalid\n",
			cmd);
		return -EINVAL;
	}

	if (tegra_platform_is_silicon()) {
		*aux_stat = tegra_dpaux_readl(dp, DPAUX_DP_AUXSTAT);
		if (!(*aux_stat & DPAUX_DP_AUXSTAT_HPD_STATUS_PLUGGED)) {
			dev_err(&dp->dc->ndev->dev, "dp: HPD is not detected\n");
			return -EFAULT;
		}
	}

	tegra_dpaux_writel(dp, DPAUX_DP_AUXADDR, addr);
	for (i = 0; i < DP_AUX_MAX_BYTES/4; ++i) {
		tegra_dpaux_writel(dp, DPAUX_DP_AUXDATA_WRITE_W(i),
			(u32)*data);
		data += 4;
	}

	reg_val = tegra_dpaux_readl(dp, DPAUX_DP_AUXCTL);
	reg_val &= ~DPAUX_DP_AUXCTL_CMD_MASK;
	reg_val |= cmd;
	reg_val &= ~DPAUX_DP_AUXCTL_CMDLEN_FIELD;
	reg_val |= (*size << DPAUX_DP_AUXCTL_CMDLEN_SHIFT);

	while ((timeout_retries > 0) && (defer_retries > 0)) {
		if ((timeout_retries != DP_AUX_TIMEOUT_MAX_TRIES) ||
		    (defer_retries != DP_AUX_DEFER_MAX_TRIES))
			usleep_range(DP_DPCP_RETRY_SLEEP_NS,
				DP_DPCP_RETRY_SLEEP_NS << 1);

		reg_val |= DPAUX_DP_AUXCTL_TRANSACTREQ_PENDING;
		tegra_dpaux_writel(dp, DPAUX_DP_AUXCTL, reg_val);

		if (tegra_dpaux_wait_transaction(dp))
			dev_err(&dp->dc->ndev->dev,
				"dp: aux write transaction timeout\n");

		*aux_stat = tegra_dpaux_readl(dp, DPAUX_DP_AUXSTAT);

		/* Ignore I2C errors on fpga */
		if (tegra_platform_is_fpga())
			*aux_stat &= ~DPAUX_DP_AUXSTAT_REPLYTYPE_I2CNACK;

		if ((*aux_stat & DPAUX_DP_AUXSTAT_TIMEOUT_ERROR_PENDING) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_RX_ERROR_PENDING) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_SINKSTAT_ERROR_PENDING) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_NO_STOP_ERROR_PENDING)) {
			if (timeout_retries-- > 0) {
				dev_dbg(&dp->dc->ndev->dev,
					"dp: aux write retry (0x%x) -- %d\n",
					*aux_stat, timeout_retries);
				/* clear the error bits */
				tegra_dpaux_writel(dp, DPAUX_DP_AUXSTAT,
					*aux_stat);
				continue;
			} else {
				dev_err(&dp->dc->ndev->dev,
					"dp: aux write got error (0x%x)\n",
					*aux_stat);
				return -EFAULT;
			}
		}

		if ((*aux_stat & DPAUX_DP_AUXSTAT_REPLYTYPE_I2CDEFER) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_REPLYTYPE_DEFER)) {
			if (defer_retries-- > 0) {
				dev_dbg(&dp->dc->ndev->dev,
					"dp: aux write defer (0x%x) -- %d\n",
					*aux_stat, defer_retries);
				/* clear the error bits */
				tegra_dpaux_writel(dp, DPAUX_DP_AUXSTAT,
					*aux_stat);
				continue;
			} else {
				dev_err(&dp->dc->ndev->dev,
					"dp: aux write defer exceeds max retries "
					"(0x%x)\n",
					*aux_stat);
				return -EFAULT;
			}
		}

		if ((*aux_stat & DPAUX_DP_AUXSTAT_REPLYTYPE_MASK) ==
			DPAUX_DP_AUXSTAT_REPLYTYPE_ACK) {
			*size = ((*aux_stat) & DPAUX_DP_AUXSTAT_REPLY_M_MASK);
			return 0;
		} else {
			dev_err(&dp->dc->ndev->dev,
				"dp: aux write failed (0x%x)\n", *aux_stat);
			return -EFAULT;
		}
	}
	/* Should never come to here */
	return -EFAULT;
}

static int __maybe_unused
tegra_dc_dpaux_write(struct tegra_dc_dp_data *dp, u32 cmd, u32 addr,
	u8 *data, u32 *size, u32 *aux_stat)
{
	u32	cur_size = 0;
	u32	finished = 0;
	int	ret	 = 0;

	do {
		cur_size = *size - finished;
		if (cur_size >= DP_AUX_MAX_BYTES)
			cur_size = DP_AUX_MAX_BYTES - 1;
		ret = tegra_dc_dpaux_write_chunk(dp, cmd, addr,
			data, &cur_size, aux_stat);

		finished += cur_size;
		addr += cur_size;
		data += cur_size;

		if (ret)
			break;
	} while (*size >= finished);

	*size = finished;
	return ret;
}

static int tegra_dc_dpaux_read_chunk(struct tegra_dc_dp_data *dp, u32 cmd,
	u32 addr, u8 *data, u32 *size, u32 *aux_stat)
{
	u32	reg_val;
	u32	timeout_retries = DP_AUX_TIMEOUT_MAX_TRIES;
	u32	defer_retries	= DP_AUX_DEFER_MAX_TRIES;

	if (*size >= DP_AUX_MAX_BYTES)
		return -EINVAL;	/* only read one chunk */

	/* Check to make sure the command is read command */
	switch (cmd) {
	case DPAUX_DP_AUXCTL_CMD_I2CRD:
	case DPAUX_DP_AUXCTL_CMD_I2CREQWSTAT:
	case DPAUX_DP_AUXCTL_CMD_MOTRD:
	case DPAUX_DP_AUXCTL_CMD_AUXRD:
		break;
	default:
		dev_err(&dp->dc->ndev->dev,
			"dp: aux read cmd 0x%x is invalid\n", cmd);
		return -EINVAL;
	}

	if (tegra_platform_is_silicon()) {
		*aux_stat = tegra_dpaux_readl(dp, DPAUX_DP_AUXSTAT);
		if (!(*aux_stat & DPAUX_DP_AUXSTAT_HPD_STATUS_PLUGGED)) {
			dev_err(&dp->dc->ndev->dev, "dp: HPD is not detected\n");
			return -EFAULT;
		}
	}

	tegra_dpaux_writel(dp, DPAUX_DP_AUXADDR, addr);

	reg_val = tegra_dpaux_readl(dp, DPAUX_DP_AUXCTL);
	reg_val &= ~DPAUX_DP_AUXCTL_CMD_MASK;
	reg_val |= cmd;
	reg_val &= ~DPAUX_DP_AUXCTL_CMDLEN_FIELD;
	reg_val |= (*size << DPAUX_DP_AUXCTL_CMDLEN_SHIFT);

	while ((timeout_retries > 0) && (defer_retries > 0)) {
		if ((timeout_retries != DP_AUX_TIMEOUT_MAX_TRIES) ||
		    (defer_retries != DP_AUX_DEFER_MAX_TRIES))
			usleep_range(DP_DPCP_RETRY_SLEEP_NS,
				DP_DPCP_RETRY_SLEEP_NS << 1);

		reg_val |= DPAUX_DP_AUXCTL_TRANSACTREQ_PENDING;
		tegra_dpaux_writel(dp, DPAUX_DP_AUXCTL, reg_val);

		if (tegra_dpaux_wait_transaction(dp))
			dev_err(&dp->dc->ndev->dev,
				"dp: aux read transaction timeout\n");

		*aux_stat = tegra_dpaux_readl(dp, DPAUX_DP_AUXSTAT);

		/* Ignore I2C errors on fpga */
		if (tegra_platform_is_fpga())
			*aux_stat &= ~DPAUX_DP_AUXSTAT_REPLYTYPE_I2CNACK;

		if ((*aux_stat & DPAUX_DP_AUXSTAT_TIMEOUT_ERROR_PENDING) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_RX_ERROR_PENDING) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_SINKSTAT_ERROR_PENDING) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_NO_STOP_ERROR_PENDING)) {
			if (timeout_retries-- > 0) {
				dev_dbg(&dp->dc->ndev->dev,
					"dp: aux read retry (0x%x) -- %d\n",
					*aux_stat, timeout_retries);
				/* clear the error bits */
				tegra_dpaux_writel(dp, DPAUX_DP_AUXSTAT,
					*aux_stat);
				continue; /* retry */
			} else {
				dev_err(&dp->dc->ndev->dev,
					"dp: aux read got error (0x%x)\n",
					*aux_stat);
				return -EFAULT;
			}
		}

		if ((*aux_stat & DPAUX_DP_AUXSTAT_REPLYTYPE_I2CDEFER) ||
			(*aux_stat & DPAUX_DP_AUXSTAT_REPLYTYPE_DEFER)) {
			if (defer_retries-- > 0) {
				dev_dbg(&dp->dc->ndev->dev,
					"dp: aux read defer (0x%x) -- %d\n",
					*aux_stat, defer_retries);
				/* clear the error bits */
				tegra_dpaux_writel(dp, DPAUX_DP_AUXSTAT,
					*aux_stat);
				continue;
			} else {
				dev_err(&dp->dc->ndev->dev,
					"dp: aux read defer exceeds max retries "
					"(0x%x)\n", *aux_stat);
				return -EFAULT;
			}
		}

		if ((*aux_stat & DPAUX_DP_AUXSTAT_REPLYTYPE_MASK) ==
			DPAUX_DP_AUXSTAT_REPLYTYPE_ACK) {
			int i;
			u32 temp_data[4];

			for (i = 0; i < DP_AUX_MAX_BYTES/4; ++i)
				temp_data[i] = tegra_dpaux_readl(dp,
					DPAUX_DP_AUXDATA_READ_W(i));

			*size = ((*aux_stat) & DPAUX_DP_AUXSTAT_REPLY_M_MASK);
			memcpy(data, temp_data, *size);

			return 0;
		} else {
			dev_err(&dp->dc->ndev->dev,
				"dp: aux read failed (0x%x\n", *aux_stat);
			return -EFAULT;
		}
	}
	/* Should never come to here */
	return -EFAULT;
}

static int tegra_dc_dpaux_read(struct tegra_dc_dp_data *dp, u32 cmd, u32 addr,
	u8 *data, u32 *size, u32 *aux_stat)
{
	u32	finished = 0;
	u32	cur_size;
	int	ret	 = 0;

	if (*size == 0) {
		dev_err(&dp->dc->ndev->dev,
			"dp: aux read size can't be 0\n");
		return -EINVAL;
	}

	do {
		cur_size = *size - finished;
		if (cur_size >= DP_AUX_MAX_BYTES)
			cur_size = DP_AUX_MAX_BYTES - 1;
		else
			cur_size -= 1;

		ret = tegra_dc_dpaux_read_chunk(dp, cmd, addr,
			data, &cur_size, aux_stat);

		if (ret)
			break;

		/* cur_size should be the real size returned */
		addr += cur_size;
		data += cur_size;
		finished += cur_size;

	} while (*size > finished);

	*size = finished;
	return ret;
}

static int tegra_dc_dp_dpcd_read(struct tegra_dc_dp_data *dp, u32 cmd,
	u8 *data_ptr)
{
	u32 size = 0;
	u32 status = 0;
	int ret;

	ret = tegra_dc_dpaux_read_chunk(dp, DPAUX_DP_AUXCTL_CMD_AUXRD,
		cmd, data_ptr, &size, &status);
	if (ret)
		dev_err(&dp->dc->ndev->dev,
			"dp: Failed to read DPCD data. CMD 0x%x, Status 0x%x\n",
			cmd, status);

	return ret;
}

static int tegra_dc_dp_dpcd_write(struct tegra_dc_dp_data *dp, u32 cmd,
	u8 data)
{
	u32 size = 0;
	u32 status = 0;
	int ret;

	ret = tegra_dc_dpaux_write_chunk(dp, DPAUX_DP_AUXCTL_CMD_AUXWR,
		cmd, &data, &size, &status);
	if (ret)
		dev_err(&dp->dc->ndev->dev,
			"dp: Failed to write DPCD data. CMD 0x%x, Status 0x%x\n",
			cmd, status);
	return ret;
}


static inline u64 tegra_div64(u64 dividend, u32 divisor)
{
	do_div(dividend, divisor);
	return dividend;
}


#ifdef CONFIG_DEBUG_FS
static int dbg_dp_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_dp_data *dp = s->private;

#define DUMP_REG(a) seq_printf(s, "%-32s  %03x	%08x\n",	\
		#a, a, tegra_dpaux_readl(dp, a))

	tegra_dc_io_start(dp->dc);
	clk_prepare_enable(dp->clk);

	DUMP_REG(DPAUX_INTR_EN_AUX);
	DUMP_REG(DPAUX_INTR_AUX);
	DUMP_REG(DPAUX_DP_AUXADDR);
	DUMP_REG(DPAUX_DP_AUXCTL);
	DUMP_REG(DPAUX_DP_AUXSTAT);
	DUMP_REG(DPAUX_HPD_CONFIG);
	DUMP_REG(DPAUX_HPD_IRQ_CONFIG);
	DUMP_REG(DPAUX_DP_AUX_CONFIG);
	DUMP_REG(DPAUX_HYBRID_PADCTL);
	DUMP_REG(DPAUX_HYBRID_SPARE);

	clk_disable_unprepare(dp->clk);
	tegra_dc_io_end(dp->dc);

	return 0;
}

static int dbg_dp_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dp_show, inode->i_private);
}

static const struct file_operations dbg_fops = {
	.open		= dbg_dp_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static struct dentry *dpdir;

static void tegra_dc_dp_debug_create(struct tegra_dc_dp_data *dp)
{
	struct dentry *retval;

	dpdir = debugfs_create_dir("tegra_dp", NULL);
	if (!dpdir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, dpdir, dp, &dbg_fops);
	if (!retval)
		goto free_out;
	return;
free_out:
	debugfs_remove_recursive(dpdir);
	dpdir = NULL;
	return;
}
#else
static inline void tegra_dc_dp_debug_create(struct tegra_dc_dp_data *dp)
{ }
#endif

static void tegra_dc_dpaux_enable(struct tegra_dc_dp_data *dp)
{
	/* clear interrupt */
	tegra_dpaux_writel(dp, DPAUX_INTR_AUX, 0xffffffff);
	/* do not enable interrupt for now. Enable them when Isr in place */
	tegra_dpaux_writel(dp, DPAUX_INTR_EN_AUX, 0x0);

	tegra_dpaux_writel(dp, DPAUX_HYBRID_PADCTL,
		DPAUX_HYBRID_PADCTL_AUX_DRVZ_OHM_50 |
		DPAUX_HYBRID_PADCTL_AUX_CMH_V0_70 |
		0x18 << DPAUX_HYBRID_PADCTL_AUX_DRVI_SHIFT |
		DPAUX_HYBRID_PADCTL_AUX_INPUT_RCV_ENABLE);

	tegra_dpaux_writel(dp, DPAUX_HYBRID_SPARE,
			DPAUX_HYBRID_SPARE_PAD_PWR_POWERUP);
}

static void tegra_dc_dp_dump_link_cfg(struct tegra_dc_dp_data *dp,
	const struct tegra_dc_dp_link_config *cfg)
{
	BUG_ON(!cfg);

	dev_info(&dp->dc->ndev->dev, "DP config: cfg_name               "\
		"cfg_value\n");
	dev_info(&dp->dc->ndev->dev, "           Lane Count             %d\n",
		cfg->max_lane_count);
	dev_info(&dp->dc->ndev->dev, "           SupportEnhancedFraming %s\n",
		cfg->support_enhanced_framing ? "Y" : "N");
	dev_info(&dp->dc->ndev->dev, "           Bandwidth              %d\n",
		cfg->max_link_bw);
	dev_info(&dp->dc->ndev->dev, "           bpp                    %d\n",
		cfg->bits_per_pixel);
	dev_info(&dp->dc->ndev->dev, "           EnhancedFraming        %s\n",
		cfg->enhanced_framing ? "Y" : "N");
	dev_info(&dp->dc->ndev->dev, "           Scramble_enabled       %s\n",
		cfg->scramble_ena ? "Y" : "N");
	dev_info(&dp->dc->ndev->dev, "           LinkBW                 %d\n",
		cfg->link_bw);
	dev_info(&dp->dc->ndev->dev, "           lane_count             %d\n",
		cfg->lane_count);
	dev_info(&dp->dc->ndev->dev, "           activespolarity        %d\n",
		cfg->activepolarity);
	dev_info(&dp->dc->ndev->dev, "           active_count           %d\n",
		cfg->active_count);
	dev_info(&dp->dc->ndev->dev, "           tu_size                %d\n",
		cfg->tu_size);
	dev_info(&dp->dc->ndev->dev, "           active_frac            %d\n",
		cfg->active_frac);
	dev_info(&dp->dc->ndev->dev, "           watermark              %d\n",
		cfg->watermark);
	dev_info(&dp->dc->ndev->dev, "           hblank_sym             %d\n",
		cfg->hblank_sym);
	dev_info(&dp->dc->ndev->dev, "           vblank_sym             %d\n",
		cfg->vblank_sym);
};

/* Calcuate if given cfg can meet the mode request. */
/* Return true if mode is possible, false otherwise. */
static bool tegra_dc_dp_calc_config(struct tegra_dc_dp_data *dp,
	const struct tegra_dc_mode *mode,
	struct tegra_dc_dp_link_config *cfg)
{
	const u32	link_rate = 27 * cfg->link_bw * 1000 * 1000;
	const u64	f	  = 100000;	/* precision factor */

	u32	num_linkclk_line; /* Number of link clocks per line */
	u64	ratio_f; /* Ratio of incoming to outgoing data rate */
	u64	frac_f;
	u64	activesym_f;	/* Activesym per TU */
	u64	activecount_f;
	u32	activecount;
	u32	activepolarity;
	u64	approx_value_f;
	u32	activefrac		  = 0;
	u64	accumulated_error_f	  = 0;
	u32	lowest_neg_activecount	  = 0;
	u32	lowest_neg_activepolarity = 0;
	u32	lowest_neg_tusize	  = 64;
	u32	num_symbols_per_line;
	u64	lowest_neg_activefrac	  = 0;
	u64	lowest_neg_error_f	  = 64 * f;
	u64	watermark_f;

	int	i;
	bool	neg;
	unsigned long rate;

	cfg->is_valid = false;

	rate = tegra_dc_pclk_round_rate(dp->sor->dc, dp->sor->dc->mode.pclk);

	if (!link_rate || !cfg->lane_count || !rate ||
		!cfg->bits_per_pixel)
		return false;

	if ((u64)rate * cfg->bits_per_pixel >=
		(u64)link_rate * 8 * cfg->lane_count)
		return false;

	num_linkclk_line = (u32)tegra_div64(
		(u64)link_rate * mode->h_active, rate);

	ratio_f = (u64)rate * cfg->bits_per_pixel * f;
	ratio_f /= 8;
	ratio_f = tegra_div64(ratio_f, link_rate * cfg->lane_count);

	for (i = 64; i >= 32; --i) {
		activesym_f	= ratio_f * i;
		activecount_f	= tegra_div64(activesym_f, (u32)f) * f;
		frac_f		= activesym_f - activecount_f;
		activecount	= (u32)tegra_div64(activecount_f, (u32)f);

		if (frac_f < (f / 2)) /* fraction < 0.5 */
			activepolarity = 0;
		else {
			activepolarity = 1;
			frac_f = f - frac_f;
		}

		if (frac_f != 0) {
			frac_f = tegra_div64((f * f),  frac_f); /* 1/fraction */
			if (frac_f > (15 * f))
				activefrac = activepolarity ? 1 : 15;
			else
				activefrac = activepolarity ?
					(u32)tegra_div64(frac_f, (u32)f) + 1 :
					(u32)tegra_div64(frac_f, (u32)f);
		}

		if (activefrac == 1)
			activepolarity = 0;

		if (activepolarity == 1)
			approx_value_f = activefrac ? tegra_div64(
				activecount_f + (activefrac * f - f) * f,
				(activefrac * f)) :
				activecount_f + f;
		else
			approx_value_f = activefrac ?
				activecount_f + tegra_div64(f, activefrac) :
				activecount_f;

		if (activesym_f < approx_value_f) {
			accumulated_error_f = num_linkclk_line *
				tegra_div64(approx_value_f - activesym_f, i);
			neg = true;
		} else {
			accumulated_error_f = num_linkclk_line *
				tegra_div64(activesym_f - approx_value_f, i);
			neg = false;
		}

		if ((neg && (lowest_neg_error_f > accumulated_error_f)) ||
			(accumulated_error_f == 0)) {
			lowest_neg_error_f = accumulated_error_f;
			lowest_neg_tusize = i;
			lowest_neg_activecount = activecount;
			lowest_neg_activepolarity = activepolarity;
			lowest_neg_activefrac = activefrac;

			if (accumulated_error_f == 0)
				break;
		}
	}

	if (lowest_neg_activefrac == 0) {
		cfg->activepolarity = 0;
		cfg->active_count   = lowest_neg_activepolarity ?
			lowest_neg_activecount : lowest_neg_activecount - 1;
		cfg->tu_size	      = lowest_neg_tusize;
		cfg->active_frac    = 1;
	} else {
		cfg->activepolarity = lowest_neg_activepolarity;
		cfg->active_count   = (u32)lowest_neg_activecount;
		cfg->tu_size	      = lowest_neg_tusize;
		cfg->active_frac    = (u32)lowest_neg_activefrac;
	}

	dev_dbg(&dp->dc->ndev->dev,
		"dp: sor configuration: polarity: %d active count: %d "
		"tu size: %d, active frac: %d\n",
		cfg->activepolarity, cfg->active_count, cfg->tu_size,
		cfg->active_frac);

	watermark_f = tegra_div64(ratio_f * cfg->tu_size * (f - ratio_f), f);
	cfg->watermark = (u32)tegra_div64(watermark_f + lowest_neg_error_f,
		f) + cfg->bits_per_pixel / 4 - 1;
	num_symbols_per_line = (mode->h_active * cfg->bits_per_pixel) /
		(8 * cfg->lane_count);
	if (cfg->watermark > 30) {
		dev_dbg(&dp->dc->ndev->dev,
			"dp: sor setting: unable to get a good tusize, "
			"force watermark to 30.\n");
		cfg->watermark = 30;
		return false;
	} else if (cfg->watermark > num_symbols_per_line) {
		dev_dbg(&dp->dc->ndev->dev,
			"dp: sor setting: force watermark to the number "
			"of symbols in the line.\n");
		cfg->watermark = num_symbols_per_line;
		return false;
	}

	/* Refer to dev_disp.ref for more information. */
	/* # symbols/hblank = ((SetRasterBlankEnd.X + SetRasterSize.Width - */
	/*                      SetRasterBlankStart.X - 7) * link_clk / pclk) */
	/*                      - 3 * enhanced_framing - Y */
	/* where Y = (# lanes == 4) 3 : (# lanes == 2) ? 6 : 12 */
	cfg->hblank_sym = (int)tegra_div64((u64)(mode->h_back_porch +
			mode->h_front_porch + mode->h_sync_width - 7)
		* link_rate, rate)
		- 3 * cfg->enhanced_framing - (12 / cfg->lane_count);

	if (cfg->hblank_sym < 0)
		cfg->hblank_sym = 0;


	/* Refer to dev_disp.ref for more information. */
	/* # symbols/vblank = ((SetRasterBlankStart.X - */
	/*                      SetRasterBlankEen.X - 25) * link_clk / pclk) */
	/*                      - Y - 1; */
	/* where Y = (# lanes == 4) 12 : (# lanes == 2) ? 21 : 39 */
	cfg->vblank_sym = (int)tegra_div64((u64)(mode->h_active - 25)
		* link_rate, rate) - (36 / cfg->lane_count) - 4;

	if (cfg->vblank_sym < 0)
		cfg->vblank_sym = 0;

	cfg->is_valid = true;
	tegra_dc_dp_dump_link_cfg(dp, cfg);

	return true;
}

static int tegra_dc_dp_init_max_link_cfg(struct tegra_dc_dp_data *dp,
	struct tegra_dc_dp_link_config *cfg)
{
	u8     dpcd_data;
	int    ret;

	CHECK_RET(tegra_dc_dp_dpcd_read(dp, NV_DPCD_MAX_LANE_COUNT,
			&dpcd_data));

	cfg->max_lane_count = dpcd_data & NV_DPCD_MAX_LANE_COUNT_MASK;
	cfg->tps3_supported =
		(dpcd_data & NV_DPCD_MAX_LANE_COUNT_TPS3_SUPPORTED_YES) ?
		true : false;

	cfg->support_enhanced_framing =
		(dpcd_data & NV_DPCD_MAX_LANE_COUNT_ENHANCED_FRAMING_YES) ?
		true : false;

	CHECK_RET(tegra_dc_dp_dpcd_read(dp, NV_DPCD_MAX_DOWNSPREAD,
			&dpcd_data));
	cfg->downspread = (dpcd_data & NV_DPCD_MAX_DOWNSPREAD_VAL_0_5_PCT) ?
		true : false;
	cfg->support_fast_lt = (dpcd_data &
		NV_DPCD_MAX_DOWNSPREAD_NO_AUX_HANDSHAKE_LT_T) ? true : false;

	CHECK_RET(tegra_dc_dp_dpcd_read(dp, NV_DPCD_TRAINING_AUX_RD_INTERVAL,
			&dpcd_data));
	cfg->aux_rd_interval = dpcd_data;

	CHECK_RET(tegra_dc_dp_dpcd_read(dp, NV_DPCD_MAX_LINK_BANDWIDTH,
			&cfg->max_link_bw));

	cfg->bits_per_pixel = dp->dc->pdata->default_out->depth;

	CHECK_RET(tegra_dc_dp_dpcd_read(dp, NV_DPCD_EDP_CONFIG_CAP,
			&dpcd_data));
	cfg->alt_scramber_reset_cap =
		(dpcd_data & NV_DPCD_EDP_CONFIG_CAP_ASC_RESET_YES) ?
		true : false;
	cfg->only_enhanced_framing =
		(dpcd_data & NV_DPCD_EDP_CONFIG_CAP_FRAMING_CHANGE_YES) ?
		true : false;
	cfg->edp_cap = (dpcd_data &
		NV_DPCD_EDP_CONFIG_CAP_DISPLAY_CONTROL_CAP_YES) ? true : false;

	cfg->lane_count	      = cfg->max_lane_count;
	cfg->link_bw	      = cfg->max_link_bw;
	cfg->enhanced_framing = cfg->support_enhanced_framing;

	tegra_dc_dp_calc_config(dp, dp->mode, cfg);
	return 0;
}

static int tegra_dc_dp_set_assr(struct tegra_dc_dp_data *dp, bool ena)
{
	int ret;

	u8 dpcd_data = ena ?
		NV_DPCD_EDP_CONFIG_SET_ASC_RESET_ENABLE :
		NV_DPCD_EDP_CONFIG_SET_ASC_RESET_DISABLE;

	CHECK_RET(tegra_dc_dp_dpcd_write(dp, NV_DPCD_EDP_CONFIG_SET,
			dpcd_data));

	/* Also reset the scrambler to 0xfffe */
	tegra_dc_sor_set_internal_panel(dp->sor, ena);
	return 0;
}


static int tegra_dp_set_link_bandwidth(struct tegra_dc_dp_data *dp, u8 link_bw)
{
	tegra_dc_sor_set_link_bandwidth(dp->sor, link_bw);

	/* Sink side */
	return tegra_dc_dp_dpcd_write(dp, NV_DPCD_LINK_BANDWIDTH_SET, link_bw);
}

static int tegra_dp_set_lane_count(struct tegra_dc_dp_data *dp,
	const struct tegra_dc_dp_link_config *cfg)
{
	u8 dpcd_data;
	int ret;

	/* check if panel support enhanched_framing */
	dpcd_data = cfg->lane_count;
	if (cfg->enhanced_framing)
		dpcd_data |= NV_DPCD_LANE_COUNT_SET_ENHANCEDFRAMING_T;
	CHECK_RET(tegra_dc_dp_dpcd_write(dp, NV_DPCD_LANE_COUNT_SET,
			dpcd_data));

	tegra_dc_sor_set_lane_count(dp->sor, cfg->lane_count);

	/* Also power down lanes that will not be used */
	return 0;
}

static bool tegra_dc_dp_link_trained(struct tegra_dc_dp_data *dp,
	const struct tegra_dc_dp_link_config *cfg)
{
	u32 lane;
	u8  mask;
	u8  data;
	int ret;

	for (lane = 0; lane < cfg->lane_count; ++lane) {
		CHECK_RET(tegra_dc_dp_dpcd_read(dp, (lane/2) ?
				NV_DPCD_LANE2_3_STATUS : NV_DPCD_LANE0_1_STATUS,
				&data));
		mask = (lane & 1) ?
			NV_DPCD_STATUS_LANEXPLUS1_CR_DONE_YES |
			NV_DPCD_STATUS_LANEXPLUS1_CHN_EQ_DONE_YES |
			NV_DPCD_STATUS_LANEXPLUS1_SYMBOL_LOCKED_YES :
			NV_DPCD_STATUS_LANEX_CR_DONE_YES |
			NV_DPCD_STATUS_LANEX_CHN_EQ_DONE_YES |
			NV_DPCD_STATUS_LANEX_SYMBOL_LOCKED_YES;
		if ((data & mask) != mask)
			return false;
	}
	return true;
}


static int tegra_dc_dp_fast_link_training(struct tegra_dc_dp_data *dp,
	const struct tegra_dc_dp_link_config *cfg)
{
	struct tegra_dc_sor_data *sor = dp->sor;
	u8	link_bw;
	u8	lane_count;
	u32	data;
	u32	size;
	u32	status;
	int	j;
	u32	mask = 0xffff >> ((4 - cfg->lane_count) * 4);

	BUG_ON(!cfg || !cfg->is_valid);

	tegra_dc_sor_set_lane_parm(sor, cfg);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_MAIN_LINK_CHANNEL_CODING_SET,
		NV_DPCD_MAIN_LINK_CHANNEL_CODING_SET_ANSI_8B10B);

	/* Send TP1 */
	tegra_dc_sor_set_dp_linkctl(sor, true, trainingPattern_1, cfg);
	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_PATTERN_SET,
		NV_DPCD_TRAINING_PATTERN_SET_TPS_TP1);

	for (j = 0; j < cfg->lane_count; ++j)
		tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_LANE0_SET + j,
			0x24);
	usleep_range(500, 1000);
	size = 2;
	tegra_dc_dpaux_read(dp, DPAUX_DP_AUXCTL_CMD_AUXRD,
		NV_DPCD_LANE0_1_STATUS, (u8 *)&data, &size, &status);
	status = mask & 0x1111;
	if ((data & status) != status) {
		dev_err(&dp->dc->ndev->dev,
			"dp: Link training error for TP1 (0x%x)\n", data);
		return -EFAULT;
	}

	/* enable ASSR */
	tegra_dc_dp_set_assr(dp, true);
	tegra_dc_sor_set_dp_linkctl(sor, true, trainingPattern_3, cfg);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_PATTERN_SET,
		cfg->link_bw == 20 ? 0x23 : 0x22);
	for (j = 0; j < cfg->lane_count; ++j)
		tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_LANE0_SET + j,
			0x24);
	usleep_range(500, 1000);

	size = 2;
	tegra_dc_dpaux_read(dp, DPAUX_DP_AUXCTL_CMD_AUXRD,
		NV_DPCD_LANE0_1_STATUS, (u8 *)&data, &size, &status);
	if ((data & mask) != (0x7777 & mask)) {
		dev_info(&dp->dc->ndev->dev,
			"dp: Link training error for TP2/3 (0x%x)\n", data);
		return -EFAULT;
	}

	tegra_dc_sor_set_dp_linkctl(sor, true, trainingPattern_Disabled, cfg);
	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_PATTERN_SET, 0);

	if (!tegra_dc_dp_link_trained(dp, cfg)) {
		tegra_dc_sor_read_link_config(dp->sor, &link_bw,
			&lane_count);
		dev_err(&dp->dc->ndev->dev,
			"Fast link trainging failed, link bw %d, lane # %d\n",
			link_bw, lane_count);
		return -EFAULT;
	} else
		dev_info(&dp->dc->ndev->dev,
			"Fast link trainging succeeded, link bw %d, lane %d\n",
			cfg->link_bw, cfg->lane_count);

	return 0;
}

static int tegra_dp_link_config(struct tegra_dc_dp_data *dp,
					struct tegra_dc_dp_link_config *cfg)
{
	u8	dpcd_data;
	u8	link_bw;
	u8	lane_count;
	u32	retry;
	int	ret;

	if (cfg->lane_count == 0) {
		/* TODO: shutdown the link */
		return 0;
	}

	/* Set power state if it is not in normal level */
	CHECK_RET(tegra_dc_dp_dpcd_read(dp, NV_DPCD_SET_POWER, &dpcd_data));
	if (dpcd_data == NV_DPCD_SET_POWER_VAL_D3_PWRDWN) {
		dpcd_data = NV_DPCD_SET_POWER_VAL_D0_NORMAL;
		retry = 3;	/* DP spec requires 3 retries */
		do {
			ret = tegra_dc_dp_dpcd_write(dp,
				NV_DPCD_SET_POWER, dpcd_data);
		} while ((--retry > 0) && ret);
		if (ret) {
			dev_err(&dp->dc->ndev->dev,
				"dp: Failed to set DP panel power\n");
			return ret;
		}
	}

	/* Enable ASSR if possible */
	if (cfg->alt_scramber_reset_cap)
		CHECK_RET(tegra_dc_dp_set_assr(dp, true));

	ret = tegra_dp_set_link_bandwidth(dp, cfg->link_bw);
	if (ret) {
		dev_err(&dp->dc->ndev->dev, "dp: Failed to set link bandwidth\n");
		return ret;
	}
	ret = tegra_dp_set_lane_count(dp, cfg);
	if (ret) {
		dev_err(&dp->dc->ndev->dev, "dp: Failed to set lane count\n");
		return ret;
	}
	tegra_dc_sor_set_dp_linkctl(dp->sor, true, trainingPattern_None, cfg);

	/* Link training rules: always try fast link training for eDP panel
	   when LT data is provided, or DP panel with valid config values */
	ret = -1;
	if ((cfg->edp_cap && dp->pdata->n_lt_settings)
		|| (cfg->support_fast_lt && cfg->vs_pe_valid)) {
		ret = tegra_dc_dp_fast_link_training(dp, cfg);
		if (ret) {
			dev_WARN(&dp->dc->ndev->dev,
				"dp: fast link training failed\n");
			cfg->vs_pe_valid = false;
		}
	}
	/* Fall back to full link training otherwise */
	if (ret) {
		ret = tegra_dp_lt(dp);
		if (ret < 0) {
			dev_err(&dp->dc->ndev->dev, "dp: link training failed\n");
			cfg->vs_pe_valid = false;
			return ret;
		}
	}

	/* Everything goes well, double check the link config */
	/* TODO: record edc/c2 data for debugging */
	tegra_dc_sor_read_link_config(dp->sor, &link_bw, &lane_count);

	if ((cfg->link_bw == link_bw) && (cfg->lane_count == lane_count))
		return 0;
	else
		return -EFAULT;
}

static int tegra_dc_dp_explore_link_cfg(struct tegra_dc_dp_data *dp,
	struct tegra_dc_dp_link_config *cfg, struct tegra_dc_mode *mode)
{
	int ret;

	if (!mode->pclk || !mode->h_active || !mode->v_active) {
		dev_err(&dp->dc->ndev->dev,
			"dp: error mode configuration");
		return -EINVAL;
	}
	if (!cfg->max_link_bw || !cfg->max_lane_count) {
		dev_err(&dp->dc->ndev->dev,
			"dp: error link configuration");
		return -EINVAL;
	}

	ret = tegra_dp_link_config(dp, cfg);
fail:
	return ret;
}

static void tegra_dc_dp_lt_worker(struct work_struct *work)
{
	struct tegra_dc_dp_data *dp =
		container_of(work, struct tegra_dc_dp_data, lt_work);

	tegra_dc_disable(dp->dc);

	if (!dp->link_cfg.is_valid ||
		tegra_dp_link_config(dp, &dp->link_cfg)) {
		/* If current config is not valid or cannot be trained,
		   needs to re-explore the possilbe config */
		if (tegra_dc_dp_init_max_link_cfg(dp, &dp->link_cfg))
			dev_err(&dp->dc->ndev->dev,
				"dp: failed to init link configuration\n");
		else if (tegra_dc_dp_explore_link_cfg(dp, &dp->link_cfg,
				dp->mode))
			dev_err(&dp->dc->ndev->dev,
				"dp irq: cannot get working config\n");
	}

	tegra_dc_enable(dp->dc);
}

static irqreturn_t tegra_dp_irq(int irq, void *ptr)
{
	struct tegra_dc_dp_data *dp = ptr;
	struct tegra_dc *dc = dp->dc;
	u32 status;
	u8 data;
	u8 clear_data = 0;

	if (tegra_platform_is_fpga())
		return IRQ_NONE;

	tegra_dc_io_start(dc);

	/* clear pending bits */
	status = tegra_dpaux_readl(dp, DPAUX_INTR_AUX);
	tegra_dpaux_writel(dp, DPAUX_INTR_AUX, status);

	if (status & DPAUX_INTR_AUX_PLUG_EVENT_PENDING)
		complete_all(&dp->hpd_plug);

	if (status & DPAUX_INTR_AUX_IRQ_EVENT_PENDING) {
		if (tegra_dc_dp_dpcd_read(dp,
					NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR,
					&data))
			dev_err(&dc->ndev->dev,
					"dp: failed to read IRQ_VECTOR\n");

		dev_dbg(&dc->ndev->dev,
			"dp irq: Handle HPD with DPCD_IRQ_VECTOR 0x%x\n",
			data);

		/* For eDP only answer auto_test_request */
		if (data & NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_AUTO_TEST_YES &&
			dp->link_cfg.is_valid) {
			/* Schedule to do the link training */
			schedule_work(&dp->lt_work);

			/* Now clear auto_test bit */
			clear_data |=
			NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR_AUTO_TEST_YES;
		}

		if (clear_data)
			tegra_dc_dp_dpcd_write(dp,
					NV_DPCD_DEVICE_SERVICE_IRQ_VECTOR,
					clear_data);
	}

	tegra_dc_io_end(dc);
	return IRQ_HANDLED;
}

static int tegra_dc_dp_init(struct tegra_dc *dc)
{
	struct tegra_dc_dp_data	*dp;
	struct resource		*res;
	struct resource		*base_res;
	void __iomem		*base;
	struct clk		*clk;
	int			 err;
	u32 irq;


	dp = kzalloc(sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	irq = platform_get_irq_byname(dc->ndev, "irq_dp");
	if (irq <= 0) {
		dev_err(&dc->ndev->dev, "dp: no irq\n");
		err = -ENOENT;
		goto err_free_dp;
	}

	res = platform_get_resource_byname(dc->ndev, IORESOURCE_MEM, "dpaux");
	if (!res) {
		dev_err(&dc->ndev->dev, "dp: no mem resources for dpaux\n");
		err = -EFAULT;
		goto err_free_dp;
	}

	base_res = request_mem_region(res->start, resource_size(res),
		dc->ndev->name);
	if (!base_res) {
		dev_err(&dc->ndev->dev, "dp: request_mem_region failed\n");
		err = -EFAULT;
		goto err_free_dp;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&dc->ndev->dev, "dp: registers can't be mapped\n");
		err = -EFAULT;
		goto err_release_resource_reg;
	}

	clk = clk_get_sys("dpaux", NULL);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&dc->ndev->dev, "dp: dc clock %s.edp unavailable\n",
			dev_name(&dc->ndev->dev));
		err = -EFAULT;
		goto err_iounmap_reg;
	}

	if (!tegra_platform_is_fpga()) {
		if (request_threaded_irq(irq, NULL, tegra_dp_irq,
					IRQF_ONESHOT, "tegra_dp", dp)) {
			dev_err(&dc->ndev->dev,
				"dp: request_irq %u failed\n", irq);
			err = -EBUSY;
			goto err_get_clk;
		}
	}
	tegra_dp_disable_irq(irq);

	dp->dc = dc;
	dp->aux_base = base;
	dp->aux_base_res = base_res;
	dp->clk = clk;
	dp->mode = &dc->mode;
	dp->sor = tegra_dc_sor_init(dc, &dp->link_cfg);
	dp->irq = irq;
	dp->pdata = dc->pdata->default_out->dp_out;

	if (IS_ERR_OR_NULL(dp->sor)) {
		err = PTR_ERR(dp->sor);
		dp->sor = NULL;
		goto err_get_clk;
	}

	INIT_WORK(&dp->lt_work, tegra_dc_dp_lt_worker);
	init_completion(&dp->hpd_plug);

	tegra_dc_set_outdata(dc, dp);
	tegra_dc_dp_debug_create(dp);

	return 0;

err_get_clk:
	clk_put(clk);
err_iounmap_reg:
	iounmap(base);
err_release_resource_reg:
	release_resource(base_res);
err_free_dp:
	kfree(dp);

	return err;
}

static void tegra_dp_hpd_config(struct tegra_dc_dp_data *dp)
{
#define TEGRA_DP_HPD_UNPLUG_MIN_US	2000
#define TEGRA_DP_HPD_PLUG_MIN_US	250
#define TEGRA_DP_HPD_IRQ_MIN_US		250

	u32 val;

	val = TEGRA_DP_HPD_PLUG_MIN_US |
		(TEGRA_DP_HPD_UNPLUG_MIN_US <<
		DPAUX_HPD_CONFIG_UNPLUG_MIN_TIME_SHIFT);
	tegra_dpaux_writel(dp, DPAUX_HPD_CONFIG, val);

	tegra_dpaux_writel(dp, DPAUX_HPD_IRQ_CONFIG, TEGRA_DP_HPD_IRQ_MIN_US);

#undef TEGRA_DP_HPD_IRQ_MIN_US
#undef TEGRA_DP_HPD_PLUG_MIN_US
#undef TEGRA_DP_HPD_UNPLUG_MIN_US
}

static int tegra_dp_hpd_plug(struct tegra_dc_dp_data *dp)
{
#define TEGRA_DP_HPD_PLUG_TIMEOUT_MS	10000
	u32 val;
	int err = 0;

	might_sleep();

	val = tegra_dpaux_readl(dp, DPAUX_DP_AUXSTAT);
	if (likely(val & DPAUX_DP_AUXSTAT_HPD_STATUS_PLUGGED))
		return 0;

	INIT_COMPLETION(dp->hpd_plug);
	tegra_dp_int_en(dp, DPAUX_INTR_EN_AUX_PLUG_EVENT_EN);
	if (!wait_for_completion_timeout(&dp->hpd_plug,
		msecs_to_jiffies(TEGRA_DP_HPD_PLUG_TIMEOUT_MS))) {
		err = -ENODEV;
		goto fail;
	}
fail:
	tegra_dp_int_dis(dp, DPAUX_INTR_EN_AUX_PLUG_EVENT_DIS);
	return err;

#undef TEGRA_DP_HPD_PLUG_TIMEOUT_MS
}

static bool tegra_dc_dp_lower_config(struct tegra_dc_dp_data *dp,
					struct tegra_dc_dp_link_config *cfg)
{
	if (cfg->link_bw == SOR_LINK_SPEED_G1_62) {
		if (cfg->max_link_bw > SOR_LINK_SPEED_G1_62)
			cfg->link_bw = SOR_LINK_SPEED_G2_7;
		cfg->lane_count /= 2;
	} else if (cfg->link_bw == SOR_LINK_SPEED_G2_7)
		cfg->link_bw = SOR_LINK_SPEED_G1_62;
	else if (cfg->link_bw == SOR_LINK_SPEED_G5_4) {
		if (cfg->lane_count == 1) {
			cfg->link_bw = SOR_LINK_SPEED_G2_7;
			cfg->lane_count = cfg->max_lane_count;
		} else
			cfg->lane_count /= 2;
	} else {
		dev_err(&dp->dc->ndev->dev,
			"dp: Error link rate %d\n", cfg->link_bw);
		return false;
	}

	if (cfg->lane_count <= 0)
		goto fail;

	if (!tegra_dc_dp_calc_config(dp, dp->mode, cfg))
		goto fail;

	tegra_dp_set_lane_count(dp, cfg);
	tegra_dp_set_link_bandwidth(dp, cfg->link_bw);

	return true;
fail:
	return false;
}

static void tegra_dp_set_tx_pu(struct tegra_dc_dp_data *dp, u32 pe[4],
				u32 vs[4], u32 pc[4])
{
	u32 n_lanes = dp->link_cfg.lane_count;
	int cnt = 1;
	u32 max_tx_pu = tegra_dp_tx_pu[pc[0]][vs[0]][pe[0]];

	if (dp->pdata && dp->pdata->tx_pu_disable) {
		tegra_sor_write_field(dp->sor,
				NV_SOR_DP_PADCTL(dp->sor->portnum),
				NV_SOR_DP_PADCTL_TX_PU_ENABLE,
				NV_SOR_DP_PADCTL_TX_PU_DISABLE);
		return;
	}

	for (; cnt < n_lanes; cnt++) {
		max_tx_pu = (max_tx_pu <
			tegra_dp_tx_pu[pc[cnt]][vs[cnt]][pe[cnt]]) ?
			tegra_dp_tx_pu[pc[cnt]][vs[cnt]][pe[cnt]] :
			max_tx_pu;
	}

	tegra_sor_write_field(dp->sor, NV_SOR_DP_PADCTL(dp->sor->portnum),
				NV_SOR_DP_PADCTL_TX_PU_VALUE_DEFAULT_MASK,
				(max_tx_pu <<
				NV_SOR_DP_PADCTL_TX_PU_VALUE_SHIFT |
				NV_SOR_DP_PADCTL_TX_PU_ENABLE));
}

static int _tegra_dp_clk_recovery(struct tegra_dc_dp_data *dp, u32 pe[4],
					u32 vs[4], u32 pc[4], bool pc_supported,
					u32 n_lanes)
{
	struct tegra_dc_sor_data *sor = dp->sor;
	u32 cnt;
	bool cr_done = true;
	u8 data_ptr;
	u32 pe_temp[4], vs_temp[4];
	u32 retry_cnt = 1;
	u32 val;
retry:
	for (cnt = 0; cnt < n_lanes; cnt++) {
		u32 mask = 0;
		u32 pe_reg, vs_reg, pc_reg;
		u32 shift = 0;
		switch (cnt) {
		case 0:
			mask = NV_SOR_PR_LANE2_DP_LANE0_MASK;
			shift = NV_SOR_PR_LANE2_DP_LANE0_SHIFT;
			break;
		case 1:
			mask = NV_SOR_PR_LANE1_DP_LANE1_MASK;
			shift = NV_SOR_PR_LANE1_DP_LANE1_SHIFT;
			break;
		case 2:
			mask = NV_SOR_PR_LANE0_DP_LANE2_MASK;
			shift = NV_SOR_PR_LANE0_DP_LANE2_SHIFT;
			break;
		case 3:
			mask = NV_SOR_PR_LANE3_DP_LANE3_MASK;
			shift = NV_SOR_PR_LANE3_DP_LANE3_SHIFT;
			break;
		default:
			dev_err(&dp->dc->ndev->dev,
				"dp: incorrect lane cnt\n");
		}
		pe_reg = tegra_dp_pe_regs[pc[cnt]][vs[cnt]][pe[cnt]];
		vs_reg = tegra_dp_vs_regs[pc[cnt]][vs[cnt]][pe[cnt]];
		pc_reg = tegra_dp_vs_regs[pc[cnt]][vs[cnt]][pe[cnt]];
		tegra_sor_write_field(sor, NV_SOR_PR(sor->portnum),
						mask, (pe_reg << shift));
		tegra_sor_write_field(sor, NV_SOR_DC(sor->portnum),
						mask, (vs_reg << shift));
		if (pc_supported) {
			tegra_sor_write_field(
					sor, NV_SOR_POSTCURSOR(sor->portnum),
					mask, (pc_reg << shift));
		}
	}
	tegra_dp_set_tx_pu(dp, pe, vs, pc);
	usleep_range(15, 20);

	for (cnt = 0; cnt < n_lanes; cnt++) {
		u32 max_vs_flag = tegra_dp_is_max_vs(pe[cnt], vs[cnt]);
		u32 max_pe_flag = tegra_dp_is_max_pe(pe[cnt], vs[cnt]);

		val = (vs[cnt] << NV_DPCD_TRAINING_LANEX_SET_DC_SHIFT) |
			(max_vs_flag ?
			NV_DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_T :
			NV_DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_F) |
			(pe[cnt] << NV_DPCD_TRAINING_LANEX_SET_PE_SHIFT) |
			(max_pe_flag ?
			NV_DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_T :
			NV_DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_F);
		tegra_dc_dp_dpcd_write(dp,
			(NV_DPCD_TRAINING_LANE0_SET + cnt), val);
	}
	if (pc_supported) {
		for (cnt = 0; cnt < n_lanes / 2; cnt++) {
			u32 max_pc_flag0 = tegra_dp_is_max_pc(pc[cnt]);
			u32 max_pc_flag1 = tegra_dp_is_max_pc(pc[cnt + 1]);
			val = (pc[cnt] << NV_DPCD_LANEX_SET2_PC2_SHIFT) |
				(max_pc_flag0 ?
				NV_DPCD_LANEX_SET2_PC2_MAX_REACHED_T :
				NV_DPCD_LANEX_SET2_PC2_MAX_REACHED_F) |
				(pc[cnt + 1] <<
				NV_DPCD_LANEXPLUS1_SET2_PC2_SHIFT) |
				(max_pc_flag1 ?
				NV_DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_T :
				NV_DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_F);
			tegra_dc_dp_dpcd_write(dp,
				(NV_DPCD_TRAINING_LANE0_1_SET2 + cnt), val);
		}
	}
	tegra_dp_wait_aux_training(dp, true);

	for (cnt = 0; cnt < n_lanes / 2; cnt++) {
		tegra_dc_dp_dpcd_read(dp,
			(NV_DPCD_LANE0_1_STATUS + cnt), &data_ptr);
		if (!(data_ptr & 0x1) ||
			!(data_ptr &
			(0x1 << NV_DPCD_STATUS_LANEXPLUS1_CR_DONE_SHIFT))) {
			cr_done = false;
			break;
		}
	}

	if (cr_done)
		return 0;

	memcpy(pe_temp, pe, sizeof(pe_temp));
	memcpy(vs_temp, vs, sizeof(vs_temp));
	for (cnt = 0; cnt < n_lanes / 2; cnt++) {
		tegra_dc_dp_dpcd_read(dp,
			(NV_DPCD_LANE0_1_ADJUST_REQ + cnt), &data_ptr);
		pe[2 * cnt] = (data_ptr & NV_DPCD_ADJUST_REQ_LANEX_PE_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEX_PE_SHIFT;
		vs[2 * cnt] = (data_ptr & NV_DPCD_ADJUST_REQ_LANEX_DC_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEX_DC_SHIFT;
		pe[1 + 2 * cnt] =
			(data_ptr & NV_DPCD_ADJUST_REQ_LANEXPLUS1_PE_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEXPLUS1_PE_SHIFT;
		vs[1 + 2 * cnt] =
			(data_ptr & NV_DPCD_ADJUST_REQ_LANEXPLUS1_DC_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEXPLUS1_DC_SHIFT;
	}
	if (pc_supported) {
		tegra_dc_dp_dpcd_read(dp,
				NV_DPCD_ADJUST_REQ_POST_CURSOR2, &data_ptr);
		for (cnt = 0; cnt < n_lanes; cnt++) {
			pc[cnt] = (data_ptr >>
			NV_DPCD_ADJUST_REQ_POST_CURSOR2_LANE_SHIFT(cnt)) &
			NV_DPCD_ADJUST_REQ_POST_CURSOR2_LANE_MASK;
		}
	}

	if (!memcmp(pe_temp, pe, sizeof(pe_temp)) &&
		!memcmp(vs_temp, vs, sizeof(vs_temp))) {
		if (retry_cnt++ >= 5)
			return -EBUSY;
		goto retry;
	}

	return _tegra_dp_clk_recovery(dp, pe, vs, pc, pc_supported, n_lanes);
}

static void tegra_dp_tpg(struct tegra_dc_dp_data *dp, u32 tp, u32 n_lanes)
{
	struct tegra_dc_sor_data *sor = dp->sor;
	u32 const tbl[][2] = {
		/* ansi8b/10b encoded, scrambled */
		{1, 1}, /* no pattern */
		{1, 0}, /* training pattern 1 */
		{1, 0}, /* training pattern 2 */
	};
	u32 cnt;
	u32 val = 0;

	for (cnt = 0; cnt < n_lanes; cnt++) {
		u32 tp_shift = NV_SOR_DP_TPG_LANE1_PATTERN_SHIFT * cnt;
		val |= tp << tp_shift |
			tbl[tp][0] << (tp_shift +
			NV_SOR_DP_TPG_LANE0_CHANNELCODING_SHIFT) |
			tbl[tp][1] << (tp_shift +
			NV_SOR_DP_TPG_LANE0_SCRAMBLEREN_SHIFT);
	}

	tegra_sor_writel(sor, NV_SOR_DP_TPG, val);
}

static int tegra_dp_clk_recovery(struct tegra_dc_dp_data *dp,
					u32 pe[4], u32 vs[4], u32 pc[4])
{
	u32 n_lanes = dp->link_cfg.lane_count;
	bool pc_supported = dp->link_cfg.tps3_supported;
	int err;

	tegra_dp_tpg(dp, trainingPattern_1, n_lanes);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_PATTERN_SET,
				(NV_DPCD_TRAINING_PATTERN_SET_TPS_TP1 |
				NV_DPCD_TRAINING_PATTERN_SET_SC_DISABLED_T));

	err = _tegra_dp_clk_recovery(dp, pe, vs, pc, pc_supported, n_lanes);

	return err;
}

static int _tegra_dp_channel_eq(struct tegra_dc_dp_data *dp, u32 pe[4],
				u32 vs[4], u32 pc[4], bool pc_supported,
				u32 n_lanes)
{
	struct tegra_dc_sor_data *sor = dp->sor;
	u32 cnt;
	u8 data_ptr;
	bool cr_done = true;
	bool ce_done = true;
	u32 retry_cnt = 1;
	u32 val;

retry:
	tegra_dp_wait_aux_training(dp, false);

	for (cnt = 0; cnt < n_lanes / 2; cnt++) {
		tegra_dc_dp_dpcd_read(dp,
			(NV_DPCD_LANE0_1_STATUS + cnt), &data_ptr);
		if (!(data_ptr & 0x1) ||
			!(data_ptr &
			(0x1 << NV_DPCD_STATUS_LANEXPLUS1_CR_DONE_SHIFT))) {
			cr_done = false;
			break;
		}
		if (!(data_ptr &
		(0x1 << NV_DPCD_STATUS_LANEX_CHN_EQ_DONE_SHIFT)) ||
		!(data_ptr &
		(0x1 << NV_DPCD_STATUS_LANEX_SYMBOL_LOCKED_SHFIT)) ||
		!(data_ptr &
		(0x1 << NV_DPCD_STATUS_LANEXPLUS1_CHN_EQ_DONE_SHIFT)) ||
		!(data_ptr &
		(0x1 << NV_DPCD_STATUS_LANEXPLUS1_SYMBOL_LOCKED_SHIFT))) {
			ce_done = false;
			break;
		}
	}
	if (cr_done && ce_done) {
		tegra_dc_dp_dpcd_read(dp,
			NV_DPCD_LANE_ALIGN_STATUS_UPDATED, &data_ptr);
		if (!(data_ptr &
			NV_DPCD_LANE_ALIGN_STATUS_INTERLANE_ALIGN_DONE_YES))
			ce_done = false;
	}

	if (!cr_done)
		goto fail;

	if (ce_done)
		return 0;

	if (++retry_cnt > 5)
		goto fail;

	for (cnt = 0; cnt < n_lanes / 2; cnt++) {
		tegra_dc_dp_dpcd_read(dp,
			(NV_DPCD_LANE0_1_ADJUST_REQ + cnt), &data_ptr);
		pe[2 * cnt] = (data_ptr & NV_DPCD_ADJUST_REQ_LANEX_PE_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEX_PE_SHIFT;
		vs[2 * cnt] = (data_ptr & NV_DPCD_ADJUST_REQ_LANEX_DC_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEX_DC_SHIFT;
		pe[1 + 2 * cnt] =
			(data_ptr & NV_DPCD_ADJUST_REQ_LANEXPLUS1_PE_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEXPLUS1_PE_SHIFT;
		vs[1 + 2 * cnt] =
			(data_ptr & NV_DPCD_ADJUST_REQ_LANEXPLUS1_DC_MASK) >>
					NV_DPCD_ADJUST_REQ_LANEXPLUS1_DC_SHIFT;
	}
	if (pc_supported) {
		tegra_dc_dp_dpcd_read(dp,
				NV_DPCD_ADJUST_REQ_POST_CURSOR2, &data_ptr);
		for (cnt = 0; cnt < n_lanes; cnt++) {
			pc[cnt] = (data_ptr >>
			NV_DPCD_ADJUST_REQ_POST_CURSOR2_LANE_SHIFT(cnt)) &
			NV_DPCD_ADJUST_REQ_POST_CURSOR2_LANE_MASK;
		}
	}

	for (cnt = 0; cnt < n_lanes; cnt++) {
		u32 mask = 0;
		u32 pe_reg, vs_reg, pc_reg;
		u32 shift = 0;
		switch (cnt) {
		case 0:
			mask = NV_SOR_PR_LANE2_DP_LANE0_MASK;
			shift = NV_SOR_PR_LANE2_DP_LANE0_SHIFT;
			break;
		case 1:
			mask = NV_SOR_PR_LANE1_DP_LANE1_MASK;
			shift = NV_SOR_PR_LANE1_DP_LANE1_SHIFT;
			break;
		case 2:
			mask = NV_SOR_PR_LANE0_DP_LANE2_MASK;
			shift = NV_SOR_PR_LANE0_DP_LANE2_SHIFT;
			break;
		case 3:
			mask = NV_SOR_PR_LANE3_DP_LANE3_MASK;
			shift = NV_SOR_PR_LANE3_DP_LANE3_SHIFT;
			break;
		default:
			dev_err(&dp->dc->ndev->dev,
				"dp: incorrect lane cnt\n");
		}
		pe_reg = tegra_dp_pe_regs[pc[cnt]][vs[cnt]][pe[cnt]];
		vs_reg = tegra_dp_vs_regs[pc[cnt]][vs[cnt]][pe[cnt]];
		pc_reg = tegra_dp_pc_regs[pc[cnt]][vs[cnt]][pe[cnt]];
		tegra_sor_write_field(sor, NV_SOR_PR(sor->portnum),
						mask, (pe_reg << shift));
		tegra_sor_write_field(sor, NV_SOR_DC(sor->portnum),
						mask, (vs_reg << shift));
		if (pc_supported) {
			tegra_sor_write_field(
					sor, NV_SOR_POSTCURSOR(sor->portnum),
					mask, (pc_reg << shift));
		}
	}
	tegra_dp_set_tx_pu(dp, pe, vs, pc);
	usleep_range(15, 20);

	for (cnt = 0; cnt < n_lanes; cnt++) {
		u32 max_vs_flag = tegra_dp_is_max_vs(pe[cnt], vs[cnt]);
		u32 max_pe_flag = tegra_dp_is_max_pe(pe[cnt], vs[cnt]);

		val = (vs[cnt] << NV_DPCD_TRAINING_LANEX_SET_DC_SHIFT) |
			(max_vs_flag ?
			NV_DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_T :
			NV_DPCD_TRAINING_LANEX_SET_DC_MAX_REACHED_F) |
			(pe[cnt] << NV_DPCD_TRAINING_LANEX_SET_PE_SHIFT) |
			(max_pe_flag ?
			NV_DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_T :
			NV_DPCD_TRAINING_LANEX_SET_PE_MAX_REACHED_F);
		tegra_dc_dp_dpcd_write(dp,
			(NV_DPCD_TRAINING_LANE0_SET + cnt), val);
	}
	if (pc_supported) {
		for (cnt = 0; cnt < n_lanes / 2; cnt++) {
			u32 max_pc_flag0 = tegra_dp_is_max_pc(pc[cnt]);
			u32 max_pc_flag1 = tegra_dp_is_max_pc(pc[cnt + 1]);
			val = (pc[cnt] << NV_DPCD_LANEX_SET2_PC2_SHIFT) |
				(max_pc_flag0 ?
				NV_DPCD_LANEX_SET2_PC2_MAX_REACHED_T :
				NV_DPCD_LANEX_SET2_PC2_MAX_REACHED_F) |
				(pc[cnt + 1] <<
				NV_DPCD_LANEXPLUS1_SET2_PC2_SHIFT) |
				(max_pc_flag1 ?
				NV_DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_T :
				NV_DPCD_LANEXPLUS1_SET2_PC2_MAX_REACHED_F);
			tegra_dc_dp_dpcd_write(dp,
				(NV_DPCD_TRAINING_LANE0_1_SET2 + cnt), val);
		}
	}
	usleep_range(150, 200);

	goto retry;
fail:
	if (tegra_dc_dp_lower_config(dp, &dp->link_cfg))
		return -EAGAIN;
	return -EBUSY;
}

static int tegra_dp_channel_eq(struct tegra_dc_dp_data *dp,
					u32 pe[4], u32 vs[4], u32 pc[4])
{
	u32 n_lanes = dp->link_cfg.lane_count;
	bool pc_supported = dp->link_cfg.tps3_supported;
	int err;
	u32 tp_src = trainingPattern_2;
	u32 tp_sink = NV_DPCD_TRAINING_PATTERN_SET_TPS_TP2;

	if (pc_supported) {
		tp_src = trainingPattern_3;
		tp_sink = NV_DPCD_TRAINING_PATTERN_SET_TPS_TP3;
	}

	tegra_dp_tpg(dp, tp_src, n_lanes);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_PATTERN_SET,
				(tp_sink |
				NV_DPCD_TRAINING_PATTERN_SET_SC_DISABLED_T));

	err = _tegra_dp_channel_eq(dp, pe, vs, pc, pc_supported, n_lanes);

	return err;
}

static int tegra_dp_lt(struct tegra_dc_dp_data *dp)
{
	struct tegra_dc_sor_data *sor = dp->sor;
	int err;
	u32 pe[4] = {
		preEmphasis_Disabled,
		preEmphasis_Disabled,
		preEmphasis_Disabled,
		preEmphasis_Disabled
	};
	u32 vs[4] = {
		driveCurrent_Level0,
		driveCurrent_Level0,
		driveCurrent_Level0,
		driveCurrent_Level0
	};
	u32 pc[4] = {
		postCursor2_Level0,
		postCursor2_Level0,
		postCursor2_Level0,
		postCursor2_Level0
	};

	tegra_sor_precharge_lanes(sor);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_MAIN_LINK_CHANNEL_CODING_SET,
			NV_DPCD_MAIN_LINK_CHANNEL_CODING_SET_ANSI_8B10B);

	tegra_dp_set_lane_count(dp, &dp->link_cfg);
	tegra_dp_set_link_bandwidth(dp, dp->link_cfg.max_link_bw);

retry_cr:
	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_PATTERN_SET,
			NV_DPCD_TRAINING_PATTERN_SET_TPS_NONE);
	tegra_dp_tpg(dp, trainingPattern_Disabled, dp->link_cfg.lane_count);
	memset(pe, preEmphasis_Disabled, sizeof(pe));
	memset(vs, driveCurrent_Level0, sizeof(vs));
	memset(pc, postCursor2_Level0, sizeof(pc));

	err = tegra_dp_clk_recovery(dp, pe, vs, pc);
	if (err < 0) {
		if (tegra_dc_dp_lower_config(dp, &dp->link_cfg))
			goto retry_cr;

		dev_err(&dp->dc->ndev->dev, "dp: clk recovery failed\n");
		goto fail;
	}

	err = tegra_dp_channel_eq(dp, pe, vs, pc);
	if (err < 0) {
		if (err == -EAGAIN)
			goto retry_cr;

		dev_err(&dp->dc->ndev->dev,
			"dp: channel equalization failed\n");
		goto fail;
	}

	tegra_dp_tpg(dp, trainingPattern_Disabled, dp->link_cfg.lane_count);

	tegra_dc_dp_dpcd_write(dp, NV_DPCD_TRAINING_PATTERN_SET,
				NV_DPCD_TRAINING_PATTERN_SET_TPS_NONE);

	/* update link config with new voltage swing and pre-emphasis */
	dp->link_cfg.preemphasis =
			tegra_sor_readl(sor, NV_SOR_PR(sor->portnum));
	dp->link_cfg.drive_current =
			tegra_sor_readl(sor, NV_SOR_DC(sor->portnum));
	dp->link_cfg.postcursor =
			tegra_sor_readl(sor, NV_SOR_POSTCURSOR(sor->portnum));
	dp->link_cfg.vs_pe_valid = true;

	return 0;
fail:
	dp->link_cfg.vs_pe_valid = false;
	return err;
}

static void tegra_dc_dp_enable(struct tegra_dc *dc)
{
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(dc);
	u8     data;
	u32    retry;
	int    ret;

	if (!tegra_is_clk_enabled(dp->clk))
		clk_prepare_enable(dp->clk);

	tegra_dc_io_start(dc);
	tegra_dc_dpaux_enable(dp);

	tegra_dp_enable_irq(dp->irq);

	tegra_dp_hpd_config(dp);
	if (tegra_dp_hpd_plug(dp) < 0) {
		dev_err(&dc->ndev->dev, "dp: hpd plug failed\n");
		return;
	}

	/* Power on panel */
	if (tegra_dc_dp_init_max_link_cfg(dp, &dp->link_cfg)) {
		dev_err(&dc->ndev->dev,
			"dp: failed to init link configuration\n");
		goto error_enable;
	}

	tegra_dc_sor_enable_dp(dp->sor);

	msleep(DP_LCDVCC_TO_HPD_DELAY_MS);

	tegra_dc_sor_set_panel_power(dp->sor, true);

	/* Write power on to DPCD */
	data = NV_DPCD_SET_POWER_VAL_D0_NORMAL;
	retry = 0;
	do {
		ret = tegra_dc_dp_dpcd_write(dp,
			NV_DPCD_SET_POWER, data);
	} while ((retry++ < DP_POWER_ON_MAX_TRIES) && ret);

	if (ret) {
		dev_err(&dp->dc->ndev->dev,
			"dp: failed to power on panel (0x%x)\n", ret);
		goto error_enable;
	}

	/* Confirm DP is plugging status */
	if (tegra_platform_is_silicon() &&
		!(tegra_dpaux_readl(dp, DPAUX_DP_AUXSTAT) &
			DPAUX_DP_AUXSTAT_HPD_STATUS_PLUGGED)) {
		dev_err(&dp->dc->ndev->dev, "dp: could not detect HPD\n");
		goto error_enable;
	}

	/* Check DP version */
	if (tegra_dc_dp_dpcd_read(dp, NV_DPCD_REV, &dp->revision))
		dev_err(&dp->dc->ndev->dev,
			"dp: failed to read the revision number from sink\n");

	tegra_dc_dp_explore_link_cfg(dp, &dp->link_cfg, dp->mode);

	tegra_dc_sor_set_power_state(dp->sor, 1);
	tegra_dc_sor_attach(dp->sor);
	dp->enabled = true;

error_enable:
	tegra_dc_io_end(dc);
	return;
}

static void tegra_dc_dp_destroy(struct tegra_dc *dc)
{
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(dc);

	if (dp->sor)
		tegra_dc_sor_destroy(dp->sor);
	clk_put(dp->clk);
	iounmap(dp->aux_base);
	release_resource(dp->aux_base_res);

	kfree(dp);
}

static void tegra_dc_dp_disable(struct tegra_dc *dc)
{
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(dc);

	if (!dp->enabled)
		return;

	tegra_dc_io_start(dc);

	tegra_dp_disable_irq(dp->irq);

	tegra_dpaux_writel(dp, DPAUX_HYBRID_SPARE,
			DPAUX_HYBRID_SPARE_PAD_PWR_POWERDOWN);

	/* Power down SOR */
	tegra_dc_sor_disable(dp->sor, false);

	clk_disable(dp->clk);

	tegra_dc_io_end(dc);
	dp->enabled = false;
}

extern struct clk *tegra_get_clock_by_name(const char *name);

static long tegra_dc_dp_setup_clk(struct tegra_dc *dc, struct clk *clk)
{
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(dc);
	struct clk		*parent_clk;

	tegra_dc_sor_setup_clk(dp->sor, clk, false);

	parent_clk = tegra_get_clock_by_name("pll_dp");
	clk_set_rate(parent_clk, 270000000);

	if (!tegra_is_clk_enabled(parent_clk))
		clk_prepare_enable(parent_clk);

	return tegra_dc_pclk_round_rate(dc, dp->sor->dc->mode.pclk);
}


static void tegra_dc_dp_suspend(struct tegra_dc *dc)
{
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(dc);

	tegra_dc_dp_disable(dc);
	dp->suspended = true;
}


static void tegra_dc_dp_resume(struct tegra_dc *dc)
{
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(dc);

	if (!dp->suspended)
		return;
	tegra_dc_dp_enable(dc);
}

static void tegra_dc_dp_modeset_notifier(struct tegra_dc *dc)
{
	struct tegra_dc_dp_data *dp = tegra_dc_get_outdata(dc);
	tegra_dc_sor_modeset_notifier(dp->sor, false);
}

struct tegra_dc_out_ops tegra_dc_dp_ops = {
	.init	   = tegra_dc_dp_init,
	.destroy   = tegra_dc_dp_destroy,
	.enable	   = tegra_dc_dp_enable,
	.disable   = tegra_dc_dp_disable,
	.suspend   = tegra_dc_dp_suspend,
	.resume	   = tegra_dc_dp_resume,
	.setup_clk = tegra_dc_dp_setup_clk,
	.modeset_notifier = tegra_dc_dp_modeset_notifier,
};


