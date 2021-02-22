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
#include <linux/mfd/syscon.h>
#include <trace/events/power.h>

#define CREATE_TRACE_POINTS
#include "trace.h"

#include "clk-regmap.h"
#include "clk-debug.h"

static struct clk_hw *measure;
static bool debug_suspend;
static struct dentry *clk_debugfs_suspend;

struct hw_debug_clk {
	struct list_head	list;
	struct clk_hw		*clk_hw;
	struct device		*dev;
};

static DEFINE_SPINLOCK(clk_reg_lock);
static DEFINE_MUTEX(clk_debug_lock);
static LIST_HEAD(clk_hw_debug_list);
static LIST_HEAD(clk_hw_debug_mux_list);

#define TCXO_DIV_4_HZ		4800000
#define SAMPLE_TICKS_1_MS	0x1000
#define SAMPLE_TICKS_27_MS	0x20000

#define XO_DIV4_CNT_DONE	BIT(25)
#define CNT_EN			BIT(20)
#define CLR_CNT			BIT(21)
#define XO_DIV4_TERM_CNT_MASK	GENMASK(19, 0)
#define MEASURE_CNT		GENMASK(24, 0)
#define CBCR_ENA		BIT(0)

#define clock_debug_output(m, c, fmt, ...)		\
do {							\
	if (m)						\
		seq_printf(m, fmt, ##__VA_ARGS__);	\
	else if (c)					\
		pr_cont(fmt, ##__VA_ARGS__);		\
	else						\
		pr_info(fmt, ##__VA_ARGS__);		\
} while (0)

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

/**
 * clk_is_debug_mux - Checks if clk is a mux clk
 *
 * @hw: clk to check on
 *
 * Iterate over maintained debug mux clk list to know
 * if concern clk is a debug mux
 *
 * Returns true on success, false otherwise.
 */
static bool clk_is_debug_mux(struct clk_hw *hw)
{
	struct clk_debug_mux *mux;

	if (hw) {
		list_for_each_entry(mux, &clk_hw_debug_mux_list, list)
			if (&mux->hw == hw)
				return true;
	}

	return false;
}

static int clk_find_and_set_parent(struct clk_hw *mux, struct clk_hw *clk)
{
	int i;

	if (!clk || !clk_is_debug_mux(mux))
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

	if (!clk_is_debug_mux(mux))
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

	if (!clk_is_debug_mux(mux))
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

	if (!clk_is_debug_mux(mux))
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

	if (clk_is_debug_mux(parent) && !mux->mux_sels) {
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


int devm_clk_register_debug_mux(struct device *pdev, struct clk_debug_mux *mux)
{
	struct clk *clk;

	if (!mux)
		return -EINVAL;

	clk = devm_clk_register(pdev, &mux->hw);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	mutex_lock(&clk_debug_lock);
	list_add(&mux->list, &clk_hw_debug_mux_list);
	mutex_unlock(&clk_debug_lock);

	return 0;
}
EXPORT_SYMBOL(devm_clk_register_debug_mux);

int clk_debug_measure_register(struct clk_hw *hw)
{
	if (IS_ERR_OR_NULL(measure)) {
		if (clk_is_debug_mux(hw)) {
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
					  struct seq_file *s, int level,
					  int num_vdd_classes,
					  struct clk_vdd_class **vdd_classes)
{
	struct clk_regmap *rclk = to_clk_regmap(hw);
	struct clk_vdd_class_data *vdd_data = &rclk->vdd_data;
	struct clk_vdd_class *vdd_class;
	int off, i, j, vdd_level;
	unsigned long rate;

	rate = clk_get_rate(hw->clk);
	vdd_level = clk_find_vdd_level(hw, vdd_data, rate);

	seq_printf(s, "%2s%10lu", vdd_level == level ? "[" : "",
		vdd_data->rate_max[level]);

	for (i = 0; i < num_vdd_classes; i++) {
		vdd_class = vdd_classes[i];
		for (j = 0; j < vdd_class->num_regulators; j++) {
			off = vdd_class->num_regulators * level + j;
			if (vdd_class->vdd_uv)
				seq_printf(s, "%10u", vdd_class->vdd_uv[off]);
		}
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
	struct clk_vdd_class **vdd_classes;
	int level = 0, i, j, vdd_level, num_classes;
	unsigned long rate;

	rate = clk_get_rate(hw->clk);
	vdd_level = clk_find_vdd_level(hw, vdd_data, rate);

	if (vdd_level < 0) {
		seq_printf(s, "could not find_vdd_level for %s, %ld\n",
			clk_hw_get_name(hw),  rate);
		return 0;
	}

	num_classes = vdd_data->num_vdd_classes;
	if (vdd_data->vdd_class)
		num_classes += 1;

	vdd_classes = kmalloc_array(num_classes, sizeof(*vdd_classes),
				    GFP_KERNEL);
	if (!vdd_classes)
		return -ENOMEM;

	for (i = 0; i < vdd_data->num_vdd_classes; i++)
		vdd_classes[i] = vdd_data->vdd_classes[i];

	if (vdd_data->vdd_class)
		vdd_classes[i] = vdd_data->vdd_class;

	seq_printf(s, "%12s", "");
	for (i = 0; i < num_classes; i++)
		for (j = 0; j < vdd_classes[i]->num_regulators; j++)
			seq_printf(s, "%10s", vdd_classes[i]->regulator_names[j]);

	seq_printf(s, "\n%12s", "freq");
	for (i = 0; i < num_classes; i++)
		for (j = 0; j < vdd_classes[i]->num_regulators; j++)
			seq_printf(s, "%10s", "uV");

	seq_puts(s, "\n");

	for (level = 0; level < vdd_data->num_rate_max; level++)
		clock_print_rate_max_by_level(hw, s, level,
					      num_classes, vdd_classes);

	kfree(vdd_classes);
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

static void clk_debug_print_hw(struct clk_hw *hw, struct seq_file *f)
{
	struct clk_regmap *rclk;

	if (IS_ERR_OR_NULL(hw))
		return;

	clk_debug_print_hw(clk_hw_get_parent(hw), f);
	seq_printf(f, "%s\n", clk_hw_get_name(hw));

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

	if (rclk->vdd_data.rate_max)
		debugfs_create_file("clk_rate_max", 0444, dentry, hw,
				    &rate_max_fops);

	if (rclk->ops && rclk->ops->list_rate)
		debugfs_create_file("clk_list_rates", 0444, dentry, hw,
				    &list_rates_fops);

	debugfs_create_file("clk_print_regs", 0444, dentry, hw,
			    &clock_print_hw_fops);

}

static int clock_debug_print_clock(struct hw_debug_clk *dclk, struct seq_file *s)
{
	char *start = "\t";
	struct clk *clk;
	struct clk_hw *clk_hw;
	unsigned long clk_rate;
	bool clk_prepared, clk_enabled;
	int vdd_level;

	if (!dclk || !dclk->clk_hw) {
		pr_err("clk param error\n");
		return 0;
	}

	if (!clk_hw_is_prepared(dclk->clk_hw))
		return 0;

	clock_debug_output(s, 0, "    ");

	clk = dclk->clk_hw->clk;

	do {
		clk_hw = __clk_get_hw(clk);
		clk_enabled = clk_hw_is_enabled(clk_hw);
		clk_prepared = clk_hw_is_prepared(clk_hw);
		clk_rate = clk_hw_get_rate(clk_hw);
		vdd_level = clk_list_rate_vdd_level(clk_hw, clk_rate);

		if (vdd_level)
			clock_debug_output(s, 1, "%s%s:%u:%u [%ld, %d]", start,
				clk_hw_get_name(clk_hw),
				clk_enabled,
				clk_prepared,
				clk_rate,
				vdd_level);
		else
			clock_debug_output(s, 1, "%s%s:%u:%u [%ld]", start,
				clk_hw_get_name(clk_hw),
				clk_enabled,
				clk_prepared,
				clk_rate);
		start = " -> ";

	} while ((clk = clk_get_parent(clk_hw->clk)));

	clock_debug_output(s, 1, "\n");

	return 1;
}

/*
 * clock_debug_print_enabled_clocks() - Print names of enabled clocks
 */
static void clock_debug_print_enabled_clocks(struct seq_file *s)
{
	struct hw_debug_clk *dclk;
	int cnt = 0;

	clock_debug_output(s, 0, "Enabled clocks:\n");

	list_for_each_entry(dclk, &clk_hw_debug_list, list)
		cnt += clock_debug_print_clock(dclk, s);

	if (cnt)
		clock_debug_output(s, 0, "Enabled clock count: %d\n", cnt);
	else
		clock_debug_output(s, 0, "No clocks enabled.\n");

}

static int enabled_clocks_show(struct seq_file *s, void *unused)
{
	mutex_lock(&clk_debug_lock);

	clock_debug_print_enabled_clocks(s);

	mutex_unlock(&clk_debug_lock);

	return 0;
}

static int enabled_clocks_open(struct inode *inode, struct file *file)
{
	return single_open(file, enabled_clocks_show, inode->i_private);
}

static const struct file_operations clk_enabled_list_fops = {
	.open		= enabled_clocks_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};


static int clock_debug_trace(struct seq_file *s, void *unused)
{
	struct hw_debug_clk *dclk;
	unsigned long clk_rate;
	bool clk_prepared, clk_enabled;
	int vdd_level;

	mutex_lock(&clk_debug_lock);

	list_for_each_entry(dclk, &clk_hw_debug_list, list) {

		clk_enabled = clk_hw_is_enabled(dclk->clk_hw);
		clk_prepared = clk_hw_is_prepared(dclk->clk_hw);
		clk_rate = clk_hw_get_rate(dclk->clk_hw);
		vdd_level = clk_list_rate_vdd_level(dclk->clk_hw, clk_rate);

		trace_clk_state(qcom_clk_hw_get_name(dclk->clk_hw),
				clk_prepared, clk_enabled,
				clk_rate, vdd_level);
	}

	mutex_unlock(&clk_debug_lock);

	return 0;
}

static int clocks_trace_open(struct inode *inode, struct file *file)
{
	return single_open(file, clock_debug_trace, inode->i_private);
}

static const struct file_operations clk_enabled_trace_fops = {
	.open		= clocks_trace_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static void clk_debug_suspend_trace_probe(void *unused,
					const char *action, int val, bool start)
{
	if (start && val > 0 && !strcmp("machine_suspend", action)) {
		pr_info("Enabled Clocks:\n");
		mutex_lock(&clk_debug_lock);
		clock_debug_print_enabled_clocks(NULL);
		mutex_unlock(&clk_debug_lock);
	}
}

static int clk_debug_suspend_enable_get(void *data, u64 *val)
{
	*val = debug_suspend;

	return 0;
}

static int clk_debug_suspend_enable_set(void *data, u64 val)
{
	int ret;

	val = !!val;
	if (val == debug_suspend)
		return 0;

	if (val)
		ret = register_trace_suspend_resume(
			clk_debug_suspend_trace_probe, NULL);
	else
		ret = unregister_trace_suspend_resume(
			clk_debug_suspend_trace_probe, NULL);
	if (ret) {
		pr_err("%s: Failed to %sregister suspend trace callback, ret=%d\n",
			__func__, val ? "" : "un", ret);
		return ret;
	}
	debug_suspend = val;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(clk_debug_suspend_enable_fops,
	clk_debug_suspend_enable_get, clk_debug_suspend_enable_set, "%llu\n");

static void clk_hw_debug_remove(struct hw_debug_clk *dclk)
{
	if (dclk) {
		list_del(&dclk->list);
		kfree(dclk);
	}
}

static void clk_debug_unregister(void)
{
	struct hw_debug_clk *dclk;

	mutex_lock(&clk_debug_lock);
	list_for_each_entry(dclk, &clk_hw_debug_list, list)
		clk_hw_debug_remove(dclk);
	mutex_unlock(&clk_debug_lock);
}

/**
 * clk_hw_debug_register - add a clk node to the debugfs clk directory
 * @clk_hw: the clk being added to the debugfs clk directory
 *
 * Dynamically adds a clk to the debugfs clk directory if debugfs has been
 * initialized.  Otherwise it bails out early since the debugfs clk directory
 * will be created lazily by clk_debug_init as part of a late_initcall.
 */
int clk_hw_debug_register(struct device *dev, struct clk_hw *clk_hw)
{
	struct hw_debug_clk *dclk = NULL;

	if (!dev || !clk_hw) {
		pr_err("%s:dev or clk_hw is NULL\n", __func__);
		return -EINVAL;
	}

	dclk = kzalloc(sizeof(*dclk), GFP_KERNEL);
	if (!dclk)
		return -ENOMEM;

	dclk->dev = dev;
	dclk->clk_hw = clk_hw;

	mutex_lock(&clk_debug_lock);
	list_add(&dclk->list, &clk_hw_debug_list);
	mutex_unlock(&clk_debug_lock);

	return 0;
}
EXPORT_SYMBOL(clk_hw_debug_register);

int clk_debug_init(void)
{
	static struct dentry *rootdir;
	int ret = 0;

	rootdir = debugfs_lookup("clk", NULL);
	if (IS_ERR_OR_NULL(rootdir)) {
		ret = PTR_ERR(rootdir);
		pr_err("%s: unable to find root clk debugfs directory, ret=%d\n",
			__func__, ret);
		return 0;
	}

	debugfs_create_file("clk_enabled_list", 0444, rootdir,
			    &clk_hw_debug_list, &clk_enabled_list_fops);

	debugfs_create_file("trace_clocks", 0444, rootdir,
			    &clk_hw_debug_list, &clk_enabled_trace_fops);

	clk_debugfs_suspend = debugfs_create_file_unsafe("debug_suspend",
						0644, rootdir, NULL,
						&clk_debug_suspend_enable_fops);
	dput(rootdir);
	if (IS_ERR(clk_debugfs_suspend)) {
		ret = PTR_ERR(clk_debugfs_suspend);
		pr_err("%s: unable to create clock debug_suspend debugfs directory, ret=%d\n",
			__func__, ret);
	}

	return ret;
}

void clk_debug_exit(void)
{
	debugfs_remove(clk_debugfs_suspend);
	if (debug_suspend)
		unregister_trace_suspend_resume(
				clk_debug_suspend_trace_probe, NULL);
	clk_debug_unregister();
}
