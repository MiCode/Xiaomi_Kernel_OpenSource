// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016, 2019-2021, The Linux Foundation. All rights reserved. */

#include <linux/clk.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/bitops.h>
#include <linux/clk/qcom.h>
#include <linux/mfd/syscon.h>

#include "clk-regmap.h"
#include "clk-debug.h"
#include "gdsc-debug.h"

static struct clk_hw *measure;

static DEFINE_SPINLOCK(clk_reg_lock);
static DEFINE_MUTEX(clk_debug_lock);

#define TCXO_DIV_4_HZ		4800000
#define SAMPLE_TICKS_1_MS	0x1000
#define SAMPLE_TICKS_27_MS	0x20000

#define XO_DIV4_CNT_DONE	BIT(25)
#define CNT_EN			BIT(20)
#define CLR_CNT			BIT(21)
#define XO_DIV4_TERM_CNT_MASK	GENMASK(19, 0)
#define MEASURE_CNT		GENMASK(24, 0)
#define CBCR_ENA		BIT(0)

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned int ticks, struct regmap *regmap,
		u32 ctl_reg, u32 status_reg)
{
	u32 regval;

	/*
	 * Clear CNT_EN to bring it to good known state and
	 * set CLK_CNT to clear previous count.
	 */
	regmap_update_bits(regmap, ctl_reg, CNT_EN, 0x0);
	regmap_update_bits(regmap, ctl_reg, CLR_CNT, CLR_CNT);

	/*
	 * Wait for timer to become ready
	 * Ideally SW should poll for MEASURE_CNT
	 * but since CLR_CNT is not available across targets
	 * add 1 us delay to let CNT clear /
	 * counter will clear within 3 reference cycle of 4.8 MHz.
	 */
	udelay(1);

	regmap_update_bits(regmap, ctl_reg, CLR_CNT, 0x0);

	/*
	 * Run measurement and wait for completion.
	 */
	regmap_update_bits(regmap, ctl_reg, XO_DIV4_TERM_CNT_MASK,
			   ticks & XO_DIV4_TERM_CNT_MASK);

	regmap_update_bits(regmap, ctl_reg, CNT_EN, CNT_EN);

	regmap_read(regmap, status_reg, &regval);

	while ((regval & XO_DIV4_CNT_DONE) == 0) {
		cpu_relax();
		regmap_read(regmap, status_reg, &regval);
	}

	regmap_update_bits(regmap, ctl_reg, CNT_EN, 0x0);

	regmap_read(regmap, status_reg, &regval);
	regval &= MEASURE_CNT;

	return regval;
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
static unsigned long clk_debug_mux_measure_rate(struct clk_hw *hw)
{
	unsigned long flags, ret = 0;
	u32 gcc_xo4_reg, multiplier = 1;
	u64 raw_count_short, raw_count_full;
	struct clk_debug_mux *meas = to_clk_measure(hw);
	struct measure_clk_data *data = meas->priv;

	clk_prepare_enable(data->cxo);

	spin_lock_irqsave(&clk_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch. */
	regmap_read(meas->regmap, data->xo_div4_cbcr, &gcc_xo4_reg);
	gcc_xo4_reg |= BIT(0);
	regmap_write(meas->regmap, data->xo_div4_cbcr, gcc_xo4_reg);

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1ms) */
	raw_count_short = run_measurement(SAMPLE_TICKS_1_MS, meas->regmap,
				data->ctl_reg, data->status_reg);

	/* Run a full measurement. (~27ms) */
	raw_count_full = run_measurement(SAMPLE_TICKS_27_MS, meas->regmap,
				data->ctl_reg, data->status_reg);

	gcc_xo4_reg &= ~BIT(0);
	regmap_write(meas->regmap, data->xo_div4_cbcr, gcc_xo4_reg);

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short)
		ret = 0;
	else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * TCXO_DIV_4_HZ;
		do_div(raw_count_full, ((SAMPLE_TICKS_27_MS * 10) + 35));
		ret = (raw_count_full * multiplier);
	}

	spin_unlock_irqrestore(&clk_reg_lock, flags);

	clk_disable_unprepare(data->cxo);

	return ret;
}

static int clk_find_and_set_parent(struct clk_hw *mux, struct clk_hw *clk)
{
	int i;

	if (!clk || !mux || !(clk_hw_get_flags(mux) & CLK_IS_MEASURE))
		return -EINVAL;

	if (!clk_set_parent(mux->clk, clk->clk))
		return 0;

	for (i = 0; i < clk_hw_get_num_parents(mux); i++) {
		struct clk_hw *parent = clk_hw_get_parent_by_index(mux, i);

		if (!clk_find_and_set_parent(parent, clk))
			return clk_set_parent(mux->clk, parent->clk);
	}

	return -EINVAL;
}

static u8 clk_debug_mux_get_parent(struct clk_hw *hw)
{
	int i, num_parents = clk_hw_get_num_parents(hw);
	struct clk_hw *hw_clk = clk_hw_get_parent(hw);
	struct clk_hw *clk_parent;
	const char *parent;

	if (!hw_clk)
		return 0;

	for (i = 0; i < num_parents; i++) {
		clk_parent = clk_hw_get_parent_by_index(hw, i);
		if (!clk_parent)
			return 0;
		parent = clk_hw_get_name(clk_parent);
		if (!strcmp(parent, clk_hw_get_name(hw_clk))) {
			pr_debug("%s: clock parent - %s, index %d\n", __func__,
				parent, i);
			return i;
		}
	}

	return 0;
}

static int clk_debug_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_debug_mux *mux = to_clk_measure(hw);
	int ret;

	if (!mux->mux_sels)
		return 0;

	/* Update the debug sel for mux */
	ret = regmap_update_bits(mux->regmap, mux->debug_offset,
		mux->src_sel_mask,
		mux->mux_sels[index] << mux->src_sel_shift);
	if (ret)
		return ret;

	/* Set the mux's post divider bits */
	return regmap_update_bits(mux->regmap, mux->post_div_offset,
		mux->post_div_mask,
		(mux->post_div_val - 1) << mux->post_div_shift);
}

const struct clk_ops clk_debug_mux_ops = {
	.get_parent = clk_debug_mux_get_parent,
	.set_parent = clk_debug_mux_set_parent,
};
EXPORT_SYMBOL(clk_debug_mux_ops);

static void enable_debug_clks(struct clk_hw *mux)
{
	struct clk_debug_mux *meas = to_clk_measure(mux);
	struct clk_hw *parent;

	if (!mux || !(clk_hw_get_flags(mux) & CLK_IS_MEASURE))
		return;

	parent = clk_hw_get_parent(mux);
	enable_debug_clks(parent);

	meas->en_mask = meas->en_mask ? meas->en_mask : CBCR_ENA;

	/* Not all muxes have a DEBUG clock. */
	if (meas->cbcr_offset != U32_MAX)
		regmap_update_bits(meas->regmap, meas->cbcr_offset,
				   meas->en_mask, meas->en_mask);
}

static void disable_debug_clks(struct clk_hw *mux)
{
	struct clk_debug_mux *meas = to_clk_measure(mux);
	struct clk_hw *parent;

	if (!mux || !(clk_hw_get_flags(mux) & CLK_IS_MEASURE))
		return;

	meas->en_mask = meas->en_mask ? meas->en_mask : CBCR_ENA;

	if (meas->cbcr_offset != U32_MAX)
		regmap_update_bits(meas->regmap, meas->cbcr_offset,
					meas->en_mask, 0);

	parent = clk_hw_get_parent(mux);
	disable_debug_clks(parent);
}

static u32 get_mux_divs(struct clk_hw *mux)
{
	struct clk_debug_mux *meas = to_clk_measure(mux);
	struct clk_hw *parent;
	u32 div_val;

	if (!mux || !(clk_hw_get_flags(mux) & CLK_IS_MEASURE))
		return 1;

	WARN_ON(!meas->post_div_val);
	div_val = meas->post_div_val;

	if (meas->pre_div_vals) {
		int i = clk_debug_mux_get_parent(mux);

		div_val *= meas->pre_div_vals[i];
	}
	parent = clk_hw_get_parent(mux);
	return div_val * get_mux_divs(parent);
}

static int clk_debug_measure_get(void *data, u64 *val)
{
	struct clk_regmap *rclk = NULL;
	struct clk_debug_mux *mux;
	struct clk_hw *hw = data;
	struct clk_hw *parent;
	int ret = 0;
	u32 regval;

	if (clk_is_regmap_clk(hw))
		rclk = to_clk_regmap(hw);

	if (rclk) {
		ret = clk_runtime_get_regmap(rclk);
		if (ret)
			return ret;
	}

	mutex_lock(&clk_debug_lock);

	ret = clk_find_and_set_parent(measure, hw);
	if (ret) {
		pr_err("Failed to set the debug mux's parent.\n");
		goto exit;
	}

	parent = clk_hw_get_parent(measure);
	if (!parent) {
		pr_err("Failed to get the debug mux's parent.\n");
		goto exit;
	}

	mux = to_clk_measure(parent);

	if ((clk_hw_get_flags(parent) & CLK_IS_MEASURE) && !mux->mux_sels) {
		regmap_read(mux->regmap, mux->period_offset, &regval);
		if (!regval) {
			pr_err("Error reading mccc period register\n");
			goto exit;
		}
		*val = 1000000000000UL;
		do_div(*val, regval);
	} else {
		enable_debug_clks(measure);
		*val = clk_debug_mux_measure_rate(measure);

		/* recursively calculate actual freq */
		*val *= get_mux_divs(measure);
		disable_debug_clks(measure);
	}
exit:
	mutex_unlock(&clk_debug_lock);
	if (rclk)
		clk_runtime_put_regmap(rclk);
	return ret;
}

DEFINE_DEBUGFS_ATTRIBUTE(clk_measure_fops, clk_debug_measure_get,
			 NULL, "%lld\n");

void clk_debug_measure_add(struct clk_hw *hw, struct dentry *dentry)
{
	debugfs_create_file("clk_measure", 0444, dentry, hw, &clk_measure_fops);
}
EXPORT_SYMBOL(clk_debug_measure_add);

int clk_debug_measure_register(struct clk_hw *hw)
{
	if (IS_ERR_OR_NULL(measure)) {
		if (clk_hw_get_flags(hw) & CLK_IS_MEASURE) {
			measure = hw;
			return 0;
		}
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(clk_debug_measure_register);

/**
 * map_debug_bases - maps each debug mux based on phandle
 * @pdev: the platform device used to find phandles
 * @base: regmap base name used to look up phandle
 * @mux: debug mux that requires a regmap
 *
 * This function attempts to look up and map a regmap for a debug mux
 * using syscon_regmap_lookup_by_phandle if the base name property exists
 * and assigns an appropriate regmap.
 *
 * Returns 0 on success, -EBADR when it can't find base name, -EERROR otherwise.
 */
int map_debug_bases(struct platform_device *pdev, const char *base,
		    struct clk_debug_mux *mux)
{
	if (!of_get_property(pdev->dev.of_node, base, NULL))
		return -EBADR;

	mux->regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						     base);
	if (IS_ERR(mux->regmap)) {
		pr_err("Failed to map %s (ret=%ld)\n", base,
				PTR_ERR(mux->regmap));
		return PTR_ERR(mux->regmap);
	}

	/*
	 * syscon_regmap_lookup_by_phandle prepares the 0th clk handle provided
	 * in the device node. The debug clock controller prepares/enables/
	 * disables the required clock, thus detach the clock.
	 */
	regmap_mmio_detach_clk(mux->regmap);

	return 0;
}
EXPORT_SYMBOL(map_debug_bases);

static void clock_print_rate_max_by_level(struct clk_hw *hw,
					  struct seq_file *s, int level)
{
	struct clk_regmap *rclk = to_clk_regmap(hw);
	struct clk_vdd_class_data *vdd_data = &rclk->vdd_data;
	struct clk_vdd_class *vdd_class = vdd_data->vdd_class;
	int off, i, vdd_level, nregs = vdd_class->num_regulators;
	unsigned long rate;

	rate = clk_get_rate(hw->clk);
	vdd_level = clk_find_vdd_level(hw, vdd_data, rate);

	seq_printf(s, "%2s%10lu", vdd_level == level ? "[" : "",
		vdd_data->rate_max[level]);

	for (i = 0; i < nregs; i++) {
		off = nregs*level + i;
		if (vdd_class->vdd_uv)
			seq_printf(s, "%10u", vdd_class->vdd_uv[off]);
	}

	if (vdd_level == level)
		seq_puts(s, "]");

	seq_puts(s, "\n");
}

static int rate_max_show(struct seq_file *s, void *unused)
{
	struct clk_hw *hw = s->private;
	struct clk_regmap *rclk = to_clk_regmap(hw);
	struct clk_vdd_class_data *vdd_data = &rclk->vdd_data;
	struct clk_vdd_class *vdd_class = vdd_data->vdd_class;
	int level = 0, i, nregs = vdd_class->num_regulators, vdd_level;
	char reg_name[10];
	unsigned long rate;

	rate = clk_get_rate(hw->clk);
	vdd_level = clk_find_vdd_level(hw, vdd_data, rate);

	if (vdd_level < 0) {
		seq_printf(s, "could not find_vdd_level for %s, %ld\n",
			clk_hw_get_name(hw),  rate);
		return 0;
	}

	seq_printf(s, "%12s", "");
	for (i = 0; i < nregs; i++) {
		snprintf(reg_name, ARRAY_SIZE(reg_name), "reg %d", i);
		seq_printf(s, "%10s", reg_name);
	}

	seq_printf(s, "\n%12s", "freq");
	for (i = 0; i < nregs; i++)
		seq_printf(s, "%10s", "uV");

	seq_puts(s, "\n");

	for (level = 0; level < vdd_data->num_rate_max; level++)
		clock_print_rate_max_by_level(hw, s, level);

	return 0;
}

static int rate_max_open(struct inode *inode, struct file *file)
{
	return single_open(file, rate_max_show, inode->i_private);
}

static const struct file_operations rate_max_fops = {
	.open		= rate_max_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int list_rates_show(struct seq_file *s, void *unused)
{
	struct clk_hw *hw = s->private;
	struct clk_regmap *rclk = to_clk_regmap(hw);
	struct clk_vdd_class_data *vdd_data = &rclk->vdd_data;
	struct clk_vdd_class *vdd_class = vdd_data->vdd_class;
	int i = 0, level;
	unsigned long rate, rate_max = 0;

	/* Find max frequency supported within voltage constraints. */
	if (!vdd_class) {
		rate_max = ULONG_MAX;
	} else {
		for (level = 0; level < vdd_data->num_rate_max; level++)
			if (vdd_data->rate_max[level])
				rate_max = vdd_data->rate_max[level];
	}

	/*
	 * List supported frequencies <= rate_max. Higher frequencies may
	 * appear in the frequency table, but are not valid and should not
	 * be listed.
	 */
	while (!IS_ERR_VALUE(rate = rclk->ops->list_rate(hw, i++, rate_max))) {
		if (rate <= 0)
			break;
		seq_printf(s, "%lu\n", rate);
	}

	return 0;
}

static int list_rates_open(struct inode *inode, struct file *file)
{
	return single_open(file, list_rates_show, inode->i_private);
}

static const struct file_operations list_rates_fops = {
	.open		= list_rates_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

void clk_debug_print_hw(struct clk_hw *hw, struct seq_file *f)
{
	struct clk_regmap *rclk;

	if (IS_ERR_OR_NULL(hw))
		return;

	clk_debug_print_hw(clk_hw_get_parent(hw), f);
	clock_debug_output(f, "%s\n", clk_hw_get_name(hw));

	if (clk_is_regmap_clk(hw)) {
		rclk = to_clk_regmap(hw);

		if (clk_runtime_get_regmap(rclk))
			return;

		if (rclk->ops && rclk->ops->list_registers)
			rclk->ops->list_registers(f, hw);

		clk_runtime_put_regmap(rclk);
	}
}

static int print_hw_show(struct seq_file *m, void *unused)
{
	struct clk_hw *hw = m->private;

	clk_debug_print_hw(hw, m);

	return 0;
}

static int print_hw_open(struct inode *inode, struct file *file)
{
	return single_open(file, print_hw_show, inode->i_private);
}

static const struct file_operations clock_print_hw_fops = {
	.open		= print_hw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

void clk_common_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct clk_regmap *rclk = to_clk_regmap(hw);

	if (rclk->vdd_data.vdd_class)
		debugfs_create_file("clk_rate_max", 0444, dentry, hw,
				    &rate_max_fops);

	if (rclk->ops && rclk->ops->list_rate)
		debugfs_create_file("clk_list_rates", 0444, dentry, hw,
				    &list_rates_fops);

	debugfs_create_file("clk_print_regs", 0444, dentry, hw,
			    &clock_print_hw_fops);

};

/**
 * qcom_clk_dump - dump the HW specific registers associated with this clock
 * and regulator
 * @clk: clock source
 * @regulator: regulator
 * @calltrace: indicates whether calltrace is required
 *
 * This function attempts to print all the registers associated with the
 * clock, it's parents and regulator.
 */
void qcom_clk_dump(struct clk *clk, struct regulator *regulator,
			bool calltrace)
{
	struct clk_hw *hw;

	if (!IS_ERR_OR_NULL(regulator))
		gdsc_debug_print_regs(regulator);

	if (IS_ERR_OR_NULL(clk))
		return;

	hw = __clk_get_hw(clk);
	if (IS_ERR_OR_NULL(hw))
		return;

	pr_info("Dumping %s Registers:\n", clk_hw_get_name(hw));
	WARN_CLK(hw, calltrace, "");
}
EXPORT_SYMBOL(qcom_clk_dump);

/**
 * qcom_clk_bulk_dump - dump the HW specific registers associated with clocks
 * and regulator
 * @num_clks: the number of clk_bulk_data
 * @clks: the clk_bulk_data table of consumer
 * @regulator: regulator source
 * @calltrace: indicates whether calltrace is required
 *
 * This function attempts to print all the registers associated with the
 * clocks in the list and regulator.
 */
void qcom_clk_bulk_dump(int num_clks, struct clk_bulk_data *clks,
			struct regulator *regulator, bool calltrace)
{
	int i;

	if (!IS_ERR_OR_NULL(regulator))
		gdsc_debug_print_regs(regulator);

	if (IS_ERR_OR_NULL(clks))
		return;

	for (i = 0; i < num_clks; i++)
		qcom_clk_dump(clks[i].clk, NULL, calltrace);
}
EXPORT_SYMBOL(qcom_clk_bulk_dump);
