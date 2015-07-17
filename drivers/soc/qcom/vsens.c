/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"VSENS %s: " fmt, __func__

#include <linux/clk.h>
#include <linux/cpu_pm.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <soc/qcom/smem.h>

#define CONFIG_0_REG		0x0
#define RESET_FUNC_BIT		BIT(31)
#define MODE_SEL_BIT		BIT(30)
#define CAPTURE_WVFRM_VAL	0x00
#define START_CAPTURE_BIT	BIT(25)
#define EN_POWER_BIT		BIT(24)
#define THRESHOLD_MAX_MASK	GENMASK(23, 16)
#define THRESHOLD_MAX_SHIFT	16
#define THRESHOLD_MIN_MASK	GENMASK(15, 8)
#define THRESHOLD_MIN_SHIFT	8
#define CAPTURE_DELAY_MASK	GENMASK(7, 4)
#define TRIG_POS_MASK		GENMASK(3, 2)
#define MID_POS			0x04

#define CONFIG_1_REG		0x4
#define JTAG_SEL_BIT		BIT(23)
#define FIFO_ADDR_MASK		GENMASK(22, 17)
#define FIFO_ADDR_SHIFT		17
#define EN_FUNC_BIT		BIT(4)
#define CLAMP_DISABLE_BIT	BIT(3)
#define STATUS_CLEAR_BIT	BIT(2)
#define EN_ALARM_BIT		BIT(1)
#define CGC_CLK_EN_BIT		BIT(0)

#define STATUS_REG		0x8
#define FIFO_COMPLETE_STS_BIT	BIT(8)
#define FIFO_DATA_MASK		GENMASK(7, 0)

#define VSENS_MAX_RAILS		6

/* SMEM fixed structure */
struct vsense_type_calib_info {
	u32			rail_type;
	u32			not_calibrated;
	u32			calib1_voltage;
	u32			calib2_voltage;
	u32			calib1_voltage_code;
	u32			calib2_voltage_code;
};

/* SMEM fixed structure */
struct vsense_type_smem_info {
	u32				version;
	u32				num_of_vsense_rails;
	struct vsense_type_calib_info	rails_calib_info[VSENS_MAX_RAILS];
};

struct vsens_chip {
	struct device		*dev;
	const char		*name;
	char			irq_min_name[64];
	char			irq_max_name[64];
	void __iomem		*reg_base;

	int			max_irq;
	int			min_irq;
	int			min_operational_floor_uv;
	int			num_corners;
	struct clk		*vsens_clk;
	u32			*corner_clk_rate;
	u32			calibration_clk_rate;

	int			voltage_corner;
	int			temp_pct;
	int			vsens_type_cal_id;
	int			code_low_voltage;
	int			low_voltage_uv;
	int			code_high_voltage;
	int			high_voltage_uv;
	struct dentry		*dir;

	struct device_node	*voltage_node;
	struct device_node	*corner_node;
	struct regulator_desc	desc_voltage;
	struct regulator_desc	desc_corner;
	struct regulator_dev	*rdev_voltage;
	struct regulator_dev	*rdev_corner;

	struct notifier_block	nb;
	int			max_count;
	int			min_count;
	spinlock_t		lock;
	u32			disabled;
	int			min_uV;
	int			max_uV;
	int			max_code;
	int			min_code;
	u32			cpu_mask;
	u32			force_panic;
};

#define MAX_CODE		0xFF
#define MIN_CODE		0x0

enum disable_reason {
	REASON_USER				= BIT(0),
	REASON_VOLTAGE_CHANGE			= BIT(1),
	REASON_POWER_COLLAPSE			= BIT(2),
	REASON_SUSPEND				= BIT(3),
	REASON_VOLTAGE_BELOW_OPERATIONAL	= BIT(4),
	REASON_INTERRUPT			= BIT(5),
	REASON_SMEM_INVALID			= BIT(6),
	REASON_CONSUMER_DISABLE			= BIT(7),
	REASON_CLOCK_CHANGE			= BIT(8),
};

static struct of_device_id msm_vsens_match_table[] = {
	{ .compatible = "qcom,msm-vsens", },
	{ },
};

static inline int get_vsens_clock_rate(struct vsens_chip *chip, int corner)
{
	if (corner > chip->num_corners) {
		pr_err("Invalid corner %d using corner %d\n",
				corner, chip->num_corners - 1);
		corner = chip->num_corners;
	}

	return chip->corner_clk_rate[corner - 1];
}

#define TEMPERATURE_PCT_DEFAULT			15

static int temp_pct_get(void *data, u64 *val)
{
	struct vsens_chip *chip = data;

	*val = chip->temp_pct;

	return 0;
}

static int temp_pct_set(void *data, u64 val)
{
	struct vsens_chip *chip = data;
	unsigned long flags;
	long pct = val;

	if (pct < 0 || pct > 100) {
		pr_err("Bad value, outside of 0 to 100, pct = %ld\n", pct);
		return -EINVAL;
	}
	spin_lock_irqsave(&chip->lock, flags);
	chip->temp_pct = pct;
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(set_temp_pct_ops, temp_pct_get, temp_pct_set,
					"%llu\n");

static void vsens_masked_write(struct vsens_chip *chip,
				int reg, u32 mask, u32 val)
{
	u32 reg_val;

	reg_val = readl_relaxed(chip->reg_base + reg);
	reg_val &= ~mask;
	reg_val |= (val & mask);
	writel_relaxed(reg_val, chip->reg_base + reg);

	/*
	 * Barrier to ensure that the reads and writes from
	 * other regulator regions (they are 1k apart) execute in
	 * order to the above write.
	 */
	mb();
}

static int scale_code_to_new_freq(struct vsens_chip *chip, int code)
{
	int scaled_code;
	u64 clk_rate = get_vsens_clock_rate(chip, chip->voltage_corner);
	u64 calib_rate = chip->calibration_clk_rate;

	scaled_code = div_u64(code * clk_rate, calib_rate);

	pr_debug("Code scaled from %x to %x for freq=%llu\n",
			code, scaled_code, clk_rate);

	return scaled_code;
}

static int scale_code_to_calib_freq(struct vsens_chip *chip, int code)
{
	int scaled_code;
	u64 clk_rate = get_vsens_clock_rate(chip, chip->voltage_corner);
	u64 calib_rate = chip->calibration_clk_rate;

	scaled_code = div_u64(code * calib_rate, clk_rate);

	pr_debug("Code scaled from %x to %x for freq=%llu\n",
			code, scaled_code, clk_rate);

	return scaled_code;
}

static int code_to_uv(struct vsens_chip *chip, int code)
{
	s64 numerator, denominator;

	if (chip->vsens_clk)
		code  = scale_code_to_new_freq(chip, code);

	numerator = (chip->code_high_voltage - code)
			* (chip->high_voltage_uv - chip->low_voltage_uv);
	denominator = (chip->code_high_voltage - chip->code_low_voltage);

	return chip->high_voltage_uv - div_s64(numerator, denominator);
}

static int uv_to_code(struct vsens_chip *chip, int uV)
{
	int code;
	s64 numerator = (chip->code_high_voltage - chip->code_low_voltage)
			* (chip->high_voltage_uv - uV);
	s64 denominator = (chip->high_voltage_uv - chip->low_voltage_uv);

	code = chip->code_high_voltage - div_s64(numerator, denominator);

	if (chip->vsens_clk)
		code = scale_code_to_calib_freq(chip, code);

	return code;
}

#define SAMPLE_DELAY_US		1
static int dump_samples_regs(struct seq_file *file, void *data)
{
	struct vsens_chip *chip = file->private;
	u32 val;
	int i;

	for (i = 0x3F; i >= 0; i--) {
		vsens_masked_write(chip, CONFIG_1_REG, FIFO_ADDR_MASK,
				i << FIFO_ADDR_SHIFT);
		val = readl_relaxed(chip->reg_base + STATUS_REG);
		val = val & FIFO_DATA_MASK;
		seq_printf(file, "0x%02x = 0x%02x = %d uV\n", i, val,
					code_to_uv(chip, val));
		/* wait few us before reading the next sample */
		usleep(SAMPLE_DELAY_US);
	}
	return 0;
}

static int dump_samples_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, dump_samples_regs, inode->i_private);
}

static const struct file_operations dump_samples_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_samples_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void update_min_max(struct vsens_chip *chip)
{
	if (chip->min_uV == -EINVAL)
		chip->min_code = 0;
	else
		chip->min_code = uv_to_code(chip, chip->min_uV);

	if (chip->max_uV == -EINVAL)
		chip->max_code = 0xFF;
	else
		chip->max_code = uv_to_code(chip, chip->max_uV);

	if (chip->min_code < MIN_CODE)
		chip->min_code = MIN_CODE;

	if (chip->max_code > MAX_CODE)
		chip->max_code = MAX_CODE;

	vsens_masked_write(chip, CONFIG_0_REG, THRESHOLD_MIN_MASK,
				chip->min_code << THRESHOLD_MIN_SHIFT);
	vsens_masked_write(chip, CONFIG_0_REG, THRESHOLD_MAX_MASK,
				chip->max_code << THRESHOLD_MAX_SHIFT);
}

static void vsens_hw_init(struct vsens_chip *chip)
{
	vsens_masked_write(chip, CONFIG_1_REG, JTAG_SEL_BIT, JTAG_SEL_BIT);
	vsens_masked_write(chip, CONFIG_0_REG, EN_POWER_BIT, EN_POWER_BIT);
	vsens_masked_write(chip, CONFIG_1_REG,
					CLAMP_DISABLE_BIT, CLAMP_DISABLE_BIT);
	vsens_masked_write(chip, CONFIG_0_REG, RESET_FUNC_BIT, RESET_FUNC_BIT);
	vsens_masked_write(chip, CONFIG_1_REG, CGC_CLK_EN_BIT, CGC_CLK_EN_BIT);
}

static void vsens_hw_init_alarm(struct vsens_chip *chip)
{
	vsens_masked_write(chip, CONFIG_0_REG, TRIG_POS_MASK, MID_POS);
	/* start capturing without delay */
	vsens_masked_write(chip, CONFIG_0_REG, CAPTURE_DELAY_MASK, 0);
	/* Don't capture unless a min/max event happens */
	vsens_masked_write(chip, CONFIG_0_REG, START_CAPTURE_BIT, 0);
	vsens_masked_write(chip, CONFIG_0_REG, MODE_SEL_BIT, CAPTURE_WVFRM_VAL);
}

#define CNFG_0_DISABLE_PORT_VAL 0x00FF0000
#define CNFG_1_DISABLE_PORT_VAL 0x00001FE0
static void vsens_hw_uninit(struct vsens_chip *chip)
{
	writel_relaxed(CNFG_0_DISABLE_PORT_VAL, chip->reg_base + CONFIG_0_REG);
	writel_relaxed(CNFG_1_DISABLE_PORT_VAL, chip->reg_base + CONFIG_1_REG);
	/*
	 * complete the writes to config registers before accessing
	 * any other registers in this block
	 */
	mb();
}

static void __vsens_enable(struct vsens_chip *chip, enum disable_reason reason)
{
	int prev_disabled;

	prev_disabled = chip->disabled;
	chip->disabled &= ~reason;
	if (prev_disabled && !chip->disabled) {
		pr_debug("%s: Enabling prev=0x%x now=0x%x\n",
				chip->name, prev_disabled, chip->disabled);
		vsens_hw_init(chip);
		vsens_hw_init_alarm(chip);
		update_min_max(chip);
		vsens_masked_write(chip, CONFIG_1_REG,
				EN_FUNC_BIT, EN_FUNC_BIT);
		vsens_masked_write(chip, CONFIG_1_REG,
				EN_ALARM_BIT, EN_ALARM_BIT);
	} else {
		pr_debug("%s: Skipping prev=0x%x now=0x%x\n",
				chip->name, prev_disabled, chip->disabled);
	}
}

static void __vsens_disable(struct vsens_chip *chip, enum disable_reason reason)
{
	int prev_disabled;

	prev_disabled = chip->disabled;
	chip->disabled |= reason;
	if (!prev_disabled && chip->disabled) {
		pr_debug("%s: Disabling prev=0x%x now=0x%x\n",
				chip->name, prev_disabled, chip->disabled);
		vsens_masked_write(chip, CONFIG_1_REG, EN_FUNC_BIT, 0);
		vsens_masked_write(chip, CONFIG_1_REG, EN_ALARM_BIT, 0);
		vsens_masked_write(chip, CONFIG_1_REG, STATUS_CLEAR_BIT, 0);
		vsens_hw_uninit(chip);
	} else {
		pr_debug("%s: Skipping prev=0x%x now=0x%x\n",
				chip->name, prev_disabled, chip->disabled);
	}
}

static void __check_operational_voltage(struct vsens_chip *chip)
{
	bool operational_now;

	operational_now = chip->min_uV >= chip->min_operational_floor_uv;

	if (operational_now == !(chip->disabled |
				REASON_VOLTAGE_BELOW_OPERATIONAL))
		return;

	if (!operational_now) {
		pr_debug("%s: floor = %d uV below operational voltage %d uV\n",
			chip->name,
			chip->min_uV, chip->min_operational_floor_uv);
		__vsens_disable(chip, REASON_VOLTAGE_BELOW_OPERATIONAL);
	} else {
		pr_debug("%s: floor = %d uV above operational voltage %d uV\n",
			chip->name,
			chip->min_uV, chip->min_operational_floor_uv);
		__vsens_enable(chip, REASON_VOLTAGE_BELOW_OPERATIONAL);
	}
}

static int msm_vsens_regulator_enable(struct regulator_dev *rdev)
{
	unsigned long flags;
	struct vsens_chip *chip = rdev_get_drvdata(rdev);

	pr_debug("%s: Enable\n", chip->name);

	spin_lock_irqsave(&chip->lock, flags);
	__vsens_enable(chip, REASON_CONSUMER_DISABLE);
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int msm_vsens_regulator_disable(struct regulator_dev *rdev)
{
	unsigned long flags;
	struct vsens_chip *chip = rdev_get_drvdata(rdev);

	pr_debug("%s: Disable\n", chip->name);

	spin_lock_irqsave(&chip->lock, flags);
	__vsens_disable(chip, REASON_CONSUMER_DISABLE);
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int msm_vsens_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	unsigned long flags;
	struct vsens_chip *chip = rdev_get_drvdata(rdev);

	spin_lock_irqsave(&chip->lock, flags);

	chip->min_uV = min_uV - (min_uV * chip->temp_pct) / 100;
	chip->max_uV = max_uV + (max_uV * chip->temp_pct) / 100;

	pr_debug("%s: New range %d %d using %d %d\n", chip->name,
			min_uV, max_uV,
			chip->min_uV, chip->max_uV);

	__check_operational_voltage(chip);
	/* apply the new voltage limits */
	__vsens_disable(chip, REASON_VOLTAGE_CHANGE);
	__vsens_enable(chip, REASON_VOLTAGE_CHANGE);

	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int msm_vsens_get_voltage(struct regulator_dev *rdev)
{
	struct vsens_chip *chip = rdev_get_drvdata(rdev);

	return chip->min_uV;
}

static int msm_vsens_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct vsens_chip *chip = rdev_get_drvdata(rdev);

	return !(chip->disabled & REASON_CONSUMER_DISABLE);
}

static int msm_vsens_set_corner(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned *selector)
{
	struct vsens_chip *chip = rdev_get_drvdata(rdev);
	unsigned long flags;
	int rc;

	pr_debug("voltage_corner changed from %d to %d\n",
				chip->voltage_corner, min_uV);

	chip->voltage_corner = min_uV;

	/* update the clk_rate */
	if (chip->vsens_clk) {
		spin_lock_irqsave(&chip->lock, flags);
		__vsens_disable(chip, REASON_CLOCK_CHANGE);
		spin_unlock_irqrestore(&chip->lock, flags);

		rc = clk_set_rate(chip->vsens_clk,
				get_vsens_clock_rate(chip, min_uV));
		if (rc < 0)
			pr_err("%s: Unable to set clk_rate to %d\n", chip->name,
				chip->corner_clk_rate[min_uV - 1]);
		else
			pr_debug("%s: clock rate set to %d for corner %d\n",
				chip->name, chip->corner_clk_rate[min_uV - 1],
				min_uV);

		spin_lock_irqsave(&chip->lock, flags);
		__vsens_enable(chip, REASON_CLOCK_CHANGE);
		spin_unlock_irqrestore(&chip->lock, flags);
	}

	return 0;
}

static int msm_vsens_get_corner(struct regulator_dev *rdev)
{
	struct vsens_chip *chip = rdev_get_drvdata(rdev);

	return chip->voltage_corner;
}

static struct regulator_ops vsens_regulator_ops_voltage = {
	.is_enabled	= msm_vsens_regulator_is_enabled,
	.enable		= msm_vsens_regulator_enable,
	.disable	= msm_vsens_regulator_disable,
	.set_voltage	= msm_vsens_set_voltage,
	.get_voltage	= msm_vsens_get_voltage,
};

static struct regulator_ops vsens_regulator_ops_corner = {
	.set_voltage	= msm_vsens_set_corner,
	.get_voltage	= msm_vsens_get_corner,
};

static void dump_regs(struct vsens_chip *chip)
{
	u32 val;

	val = readl_relaxed(chip->reg_base + CONFIG_0_REG);
	dev_err(chip->dev, "%s CNFG0 (0x%lx) = 0x%08x\n", chip->name,
			(unsigned long)chip->reg_base + CONFIG_0_REG, val);

	val = readl_relaxed(chip->reg_base + CONFIG_1_REG);
	dev_err(chip->dev, "%s CNFG1 (0x%lx) = 0x%08x\n", chip->name,
			(unsigned long)chip->reg_base + CONFIG_1_REG, val);

	val = readl_relaxed(chip->reg_base + STATUS_REG);
	dev_err(chip->dev, "%s STATUS (0x%lx) = 0x%08x\n", chip->name,
			(unsigned long)chip->reg_base + STATUS_REG, val);
}

static irqreturn_t min_max_handler(int irq, void *data)
{
	struct vsens_chip *chip = data;
	bool is_max = (irq == chip->max_irq);
	char *min_max_str = (is_max ? "max" : "min");
	char *result;
	u32 val;
	unsigned long flags;
	int i;

	dev_crit(chip->dev, "%s interrupt triggered for %s\n", min_max_str,
			chip->name);
	spin_lock_irqsave(&chip->lock, flags);
	dump_regs(chip);

	if (is_max)
		chip->max_count++;
	else
		chip->min_count++;

	dev_crit(chip->dev,
		"%s_event %s_count = %d min=(%d uV, 0x%x) max=(%d uV, 0x%x)\n",
		min_max_str, min_max_str,
		is_max ? chip->max_count : chip->min_count,
		chip->min_uV, chip->min_code,
		chip->max_uV, chip->max_code);

	/* wait for fifo capture to complete */
	val = readl_relaxed(chip->reg_base + STATUS_REG);
	while (!(val & FIFO_COMPLETE_STS_BIT)) {
		udelay(1);
		val = readl_relaxed(chip->reg_base + STATUS_REG);
	}

	/* dump the samples */
	for (i = 0x3F; i >= 0; i--) {
		vsens_masked_write(chip, CONFIG_1_REG, FIFO_ADDR_MASK,
				i << FIFO_ADDR_SHIFT);
		val = readl_relaxed(chip->reg_base + STATUS_REG);
		val = val & FIFO_DATA_MASK;
		if ((is_max && (val > chip->max_code))
			|| (!is_max && (val < chip->min_code)))
			result = "bad";
		else
			result = "";

		dev_err(chip->dev, "0x%02x = 0x%02x = %d uV %s\n", i, val,
					code_to_uv(chip, val), result);
		/* wait few us before reading the next sample */
		udelay(SAMPLE_DELAY_US);
	}
	__vsens_disable(chip, REASON_INTERRUPT);
	__vsens_enable(chip, REASON_INTERRUPT);
	spin_unlock_irqrestore(&chip->lock, flags);

	if (chip->force_panic) {
		dev_crit(chip->dev, "%s: BUG %s-threshold Triggered!\n",
				chip->name, is_max ? "MAX" : "MIN");
		BUG();
	}

	return IRQ_HANDLED;
}

static char *action_to_str(enum cpu_pm_event evt)
{
	switch (evt) {
	case CPU_PM_ENTER:
		return "CPU_PM_ENTER";
	case CPU_PM_ENTER_FAILED:
		return "CPU_PM_ENTER_FAILED";
	case CPU_PM_EXIT:
		return "CPU_PM_EXIT";
	case CPU_CLUSTER_PM_ENTER:
		return "CPU_CLUSTER_PM_ENTER";
	case CPU_CLUSTER_PM_ENTER_FAILED:
		return "CPU_CLUSTER_PM_ENTER_FAILED";
	case CPU_CLUSTER_PM_EXIT:
		return "CPU_CLUSTER_PM_EXIT";
	default:
		return "UNKNOWN";
	}
}

static void __vsens_cpu_pm_notify(struct vsens_chip *chip,
			unsigned long action)
{
	bool rail_on, rail_on_prev;
	int this_cpu = smp_processor_id();

	if (!(BIT(this_cpu) & chip->cpu_mask))
		return;

	rail_on_prev = !(chip->disabled & REASON_POWER_COLLAPSE);

	if (action == CPU_CLUSTER_PM_ENTER)
		rail_on = false;
	else
		rail_on = true;

	if (rail_on == rail_on_prev)
		return;

	pr_debug("%s: Enter on cpu = %d, action = %s rail_on = %d -> %d\n",
					chip->name,
					smp_processor_id(),
					action_to_str(action),
					rail_on_prev,
					rail_on);

	if (rail_on == false)
		__vsens_disable(chip, REASON_POWER_COLLAPSE);
	else
		__vsens_enable(chip, REASON_POWER_COLLAPSE);

	pr_debug("%s: Exit on cpu = %d, action = %s rail_on = %d -> %d\n",
					chip->name,
					smp_processor_id(),
					action_to_str(action),
					rail_on_prev,
					rail_on);
}

static int vsens_cpu_pm_notify(struct notifier_block *blk,
			unsigned long action,
			void *v)
{
	struct vsens_chip *chip = container_of(blk, struct vsens_chip, nb);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	__vsens_cpu_pm_notify(chip, action);
	spin_unlock_irqrestore(&chip->lock, flags);

	return NOTIFY_OK;
}

static int get_disabled(void *data, u64 *val)
{
	struct vsens_chip *chip = data;

	*val = chip->disabled;

	return 0;
}

static int set_disabled(void *data, u64 disable)
{
	int rc = 0;
	unsigned long flags;
	struct vsens_chip *chip = data;


	if (!!disable == !!(chip->disabled & REASON_USER))
		goto out;

	if (!disable) {
		/* user wants to enable */
		if (chip->vsens_clk) {
			rc = clk_prepare_enable(chip->vsens_clk);
			if (rc < 0) {
				pr_err("Unable to enable VSENS clk rc=%d\n",
						rc);
				goto out;
			}
		}
		/*
		 * since the state of the rail is not known at this point
		 * disable it assuming the rail is power collapsed. The sensor
		 * will be enabled once a PM_EXIT notification comes in.
		 */

		spin_lock_irqsave(&chip->lock, flags);
		__vsens_disable(chip, REASON_POWER_COLLAPSE);
		rc = cpu_pm_register_notifier(&chip->nb);
		if (rc < 0) {
			pr_err("pm notifier registration failed, rc=%d\n", rc);
			spin_unlock_irqrestore(&chip->lock, flags);
			goto out;
		}
		__vsens_enable(chip, REASON_USER);
		spin_unlock_irqrestore(&chip->lock, flags);
	} else {
		/* user wants to disable */
		spin_lock_irqsave(&chip->lock, flags);
		__vsens_disable(chip, REASON_USER);
		rc = cpu_pm_unregister_notifier(&chip->nb);
		if (rc < 0)
			pr_err("pm notifier unregistration failed, rc=%d\n",
					rc);
		spin_unlock_irqrestore(&chip->lock, flags);

		if (chip->vsens_clk)
			clk_disable_unprepare(chip->vsens_clk);
	}
out:
	pr_debug("disabled = 0x%x\n", chip->disabled);
	return rc;
}
DEFINE_SIMPLE_ATTRIBUTE(disable_ops, get_disabled, set_disabled, "0x%04llx\n");

static int show_voltage_range(struct seq_file *file, void *data)
{
	struct vsens_chip *chip = file->private;

	seq_printf(file, "min=(%d uV,0x%x) max=(%d uV,0x%x)\n",
			chip->min_uV, chip->min_code,
			chip->max_uV, chip->max_code);

	return 0;
}

static int voltage_range_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_voltage_range, inode->i_private);
}

static const struct file_operations voltage_range_ops = {
	.owner		= THIS_MODULE,
	.open		= voltage_range_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int vsens_show_regs(struct seq_file *file, void *data)
{
	struct vsens_chip *chip = file->private;
	u32 reg_val;

	reg_val = readl_relaxed(chip->reg_base + CONFIG_0_REG);
	seq_printf(file, "CNFG0 (0x%lx) = 0x%08x\n",
			(unsigned long)chip->reg_base + CONFIG_0_REG, reg_val);

	reg_val = readl_relaxed(chip->reg_base + CONFIG_1_REG);
	seq_printf(file, "CNFG1 (0x%lx) = 0x%08x\n",
			(unsigned long)chip->reg_base + CONFIG_1_REG, reg_val);

	reg_val = readl_relaxed(chip->reg_base + STATUS_REG);
	seq_printf(file, "STATUS (0x%lx) = 0x%08x\n",
			(unsigned long)chip->reg_base + STATUS_REG, reg_val);

	return 0;
}

static int vsens_regs_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, vsens_show_regs, inode->i_private);
}

static const struct file_operations regs_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= vsens_regs_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debug_fs_nodes(struct vsens_chip *chip)
{
	struct dentry *ent;

	chip->dir = debugfs_create_dir(chip->name, NULL);
	if (!chip->dir) {
		dev_err(chip->dev, "Couldn't create %s directory\n",
							chip->name);
		return -EINVAL;
	}

	ent = debugfs_create_file("disable", S_IRUGO | S_IWUSR,
				chip->dir, chip, &disable_ops);
	if (!ent) {
		dev_err(chip->dev, "Couldn't create disable debug file\n");
		goto err;
	}

	ent = debugfs_create_file("voltage_range", S_IRUGO,
			chip->dir, chip, &voltage_range_ops);
	if (!ent) {
		dev_err(chip->dev, "Couldn't create voltage_range debug file\n");
		goto err;
	}

	ent = debugfs_create_file("samples", S_IRUGO,
			chip->dir, chip, &dump_samples_debugfs_ops);
	if (!ent) {
		dev_err(chip->dev, "Couldn't create samples debug file\n");
		goto err;
	}

	ent = debugfs_create_file("temperature_pct", S_IRUGO | S_IWUSR,
				chip->dir, chip, &set_temp_pct_ops);
	if (!ent) {
		dev_err(chip->dev, "Couldn't create temperature_pct file\n");
		goto err;
	}

	ent = debugfs_create_file("registers", S_IRUGO,
			chip->dir, chip, &regs_debugfs_ops);
	if (!ent) {
		dev_err(chip->dev, "Couldn't create register debug file\n");
		goto err;
	}

	ent = debugfs_create_bool("force_panic", S_IRUGO | S_IWUSR,
				chip->dir, &(chip->force_panic));
	if (!ent) {
		dev_err(chip->dev, "Couldn't create force_panic debug file\n");
		goto err;
	}

	return 0;
err:
	debugfs_remove_recursive(chip->dir);
	return -EINVAL;
}

static int dump_calib_info(struct vsens_chip *chip,
		struct vsense_type_calib_info *info)
{
	pr_debug("rail = %d\n", info->rail_type);
	pr_debug("not_calibrated = %d\n", info->not_calibrated);
	pr_debug("calib1_voltage = %d\n", info->calib1_voltage);
	pr_debug("calib2_voltage = %d\n", info->calib2_voltage);
	pr_debug("calib1_voltage_code = %d\n", info->calib1_voltage_code);
	pr_debug("calib2_voltage_code = %d\n", info->calib2_voltage_code);
	return 0;
}

static void dump_cal_data(struct vsens_chip *chip,
		struct vsense_type_smem_info *vsens_info)
{
	int i;
	struct vsense_type_calib_info *info;

	pr_debug("num_of_rails = %d\n", vsens_info->num_of_vsense_rails);
	for (i = 0; i < vsens_info->num_of_vsense_rails; i++) {
		info = &(vsens_info->rails_calib_info[i]);
		dump_calib_info(chip, info);
	}
}

static int get_cal_data(struct platform_device *pdev,
		struct vsens_chip *chip,
		struct vsense_type_smem_info *vsens_info)
{
	int i;
	struct vsense_type_calib_info *found = NULL;

	dump_cal_data(chip, vsens_info);
	for (i = 0; i < vsens_info->num_of_vsense_rails; i++)
		if (chip->vsens_type_cal_id
			== vsens_info->rails_calib_info[i].rail_type) {
			found = &vsens_info->rails_calib_info[i];
			break;
		}

	if (found == NULL) {
		dev_err(&pdev->dev, "missing calibration data\n");
		return -EINVAL;
	}

	if (!found->calib1_voltage || !found->calib2_voltage
		|| !found->calib1_voltage_code || !found->calib2_voltage_code) {
		dev_err(&pdev->dev, "calib1_voltage = %d\n",
				found->calib1_voltage);
		dev_err(&pdev->dev, "calib2_voltage = %d\n",
				found->calib2_voltage);
		dev_err(&pdev->dev, "calib1_voltage_code = %d\n",
				found->calib1_voltage_code);
		dev_err(&pdev->dev, "calib2_voltage_code = %d\n",
				found->calib2_voltage_code);
		dev_err(&pdev->dev, "bad cal data\n");
		return -EINVAL;
	}

	if (found->calib1_voltage < found->calib2_voltage) {
		chip->low_voltage_uv = found->calib1_voltage;
		chip->high_voltage_uv = found->calib2_voltage;
		chip->code_low_voltage = found->calib1_voltage_code;
		chip->code_high_voltage = found->calib2_voltage_code;
	} else if (found->calib1_voltage > found->calib2_voltage) {
		chip->low_voltage_uv = found->calib2_voltage;
		chip->high_voltage_uv = found->calib1_voltage;
		chip->code_low_voltage = found->calib2_voltage_code;
		chip->code_high_voltage = found->calib1_voltage_code;
	} else {
		dev_err(&pdev->dev, "bad calib1, calib2 voltage-they both are %d uV\n",
				found->calib2_voltage);
		return -EINVAL;
	}

	return 0;
}

static int msm_vsens_parse_dt(struct platform_device *pdev,
				struct vsens_chip *chip,
				struct regulator_init_data **init_data_voltage,
				struct regulator_init_data **init_data_corner,
				struct resource **res)
{
	struct device *dev = &pdev->dev;
	struct device_node *cpu_node, *node = dev->of_node;
	int rc, i, cpu, num_cpus = 0;

	rc = of_property_read_string(node, "label", &chip->name);
	if (rc) {
		pr_err("Unable to find 'label' rc=%d\n", rc);
		return rc;
	}

	if (!of_find_property(node, "qcom,vsens-cpus", &num_cpus)) {
		pr_err("Unable to find 'qcom,vsens-cpus'\n");
		return -EINVAL;
	}
	num_cpus /= sizeof(u32);

	for (i = 0; i < num_cpus; i++) {
		cpu_node = of_parse_phandle(dev->of_node, "qcom,vsens-cpus", i);
		if (!cpu_node) {
			pr_err("could not find CPU node %d\n", i);
			return -EINVAL;
		}
		for_each_possible_cpu(cpu) {
			if (of_get_cpu_node(cpu, NULL) == cpu_node) {
				chip->cpu_mask |= BIT(cpu);
				break;
			}
		}
		of_node_put(cpu_node);
	}

	rc = of_property_read_u32(node, "qcom,vsens-cal-id",
					&chip->vsens_type_cal_id);
	if (rc) {
		pr_err("Unable to read 'qcom,vsens-cal-id' rc=%d\n", rc);
		return rc;
	}

	if (of_find_property(node, "clock-names", NULL)) {
		chip->vsens_clk = devm_clk_get(dev, "vsens_clk");
		if (IS_ERR(chip->vsens_clk)) {
			rc = PTR_ERR(chip->vsens_clk);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "unable to request vsens clock, rc=%d\n",
					rc);
			return rc;
		}

		rc = of_property_read_u32(node, "qcom,num-corners",
						&chip->num_corners);
		if (rc < 0) {
			dev_err(dev, "missing 'qcom,num-corners' rc=%d\n", rc);
			return rc;
		}

		chip->corner_clk_rate = devm_kcalloc(dev, chip->num_corners,
				sizeof(*(chip->corner_clk_rate)), GFP_KERNEL);
		if (!chip->corner_clk_rate)
			return -ENOMEM;

		rc = of_property_read_u32_array(node, "qcom,corner-clock-rate",
				chip->corner_clk_rate, chip->num_corners);
		if (rc < 0) {
			dev_err(dev, "Unable to get 'qcom,corner-clock-rate' rc=%d\n",
					rc);
			return rc;
		}

		rc = of_property_read_u32(node, "qcom,calib-clock-rate",
					&chip->calibration_clk_rate);
		if (rc < 0) {
			dev_err(dev, "Unable to get 'qcom,calib-clock-rate' rc=%d\n",
					rc);
			return rc;
		}
	}

	*res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "base");
	if (*res == NULL) {
		dev_err(dev, "missing base register address\n");
		return -EINVAL;
	}

	chip->voltage_node = of_get_child_by_name(node, "qcom,vsens-voltage");
	if (chip->voltage_node == NULL) {
		dev_err(dev, "missing voltage regulator node\n");
		return -EINVAL;
	}

	chip->corner_node = of_get_child_by_name(node, "qcom,vsens-corner");
	if (chip->corner_node == NULL) {
		dev_err(dev, "missing corner regulator node\n");
		return -EINVAL;
	}

	*init_data_voltage = of_get_regulator_init_data(dev,
					chip->voltage_node);
	if (*init_data_voltage == NULL) {
		dev_err(dev, "Couldn't parse voltage regulator init data\n");
		return -EINVAL;
	}

	*init_data_corner = of_get_regulator_init_data(dev,
					chip->corner_node);
	if (*init_data_corner == NULL) {
		dev_err(dev, "Couldn't parse corner regulator init data\n");
		return -EINVAL;
	}

	chip->max_irq = platform_get_irq_byname(pdev, "max");
	chip->min_irq = platform_get_irq_byname(pdev, "min");

	chip->temp_pct = TEMPERATURE_PCT_DEFAULT;
	of_property_read_u32(node, "qcom,temperature-pct",
					&chip->temp_pct);

	of_property_read_u32(node, "qcom,min-operational-floor-uv",
			&chip->min_operational_floor_uv);

	return 0;
}

static int msm_vsens_probe(struct platform_device *pdev)
{
	int rc;
	struct device *dev = &pdev->dev;
	struct vsens_chip *chip;
	struct regulator_config reg_config_voltage = {};
	struct regulator_config reg_config_corner = {};
	struct regulator_init_data *init_data_voltage;
	struct regulator_init_data *init_data_corner;
	struct resource *res;
	struct vsense_type_smem_info *vsens_info;
	unsigned temp_sz;
	bool smem_invalid = false;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->max_code = MAX_CODE;
	chip->min_code = MIN_CODE;
	chip->min_uV = -EINVAL;
	chip->max_uV = -EINVAL;

	spin_lock_init(&chip->lock);
	chip->nb.notifier_call = vsens_cpu_pm_notify;
	chip->disabled = REASON_USER
			| REASON_VOLTAGE_BELOW_OPERATIONAL
			| REASON_CONSUMER_DISABLE
			| REASON_POWER_COLLAPSE;

	rc = msm_vsens_parse_dt(pdev, chip,
			&init_data_voltage, &init_data_corner, &res);
	if (rc < 0) {
		dev_err(dev, "Couldn't parse device tree rc = %d\n", rc);
		goto err;
	}

	vsens_info = smem_get_entry(SMEM_VSENSE_DATA, &temp_sz, 0,
						SMEM_ANY_HOST_FLAG);
	if (!vsens_info) {
		dev_err(dev, "VSENS SMEM calibration data missing\n");
		smem_invalid = true;
	} else if (IS_ERR(vsens_info)) {
		rc = PTR_ERR(vsens_info);
		if (rc == -EPROBE_DEFER) {
			/* retry */
			goto err;
		} else {
			dev_err(dev, "Couldn't get smem struct rc = %d\n", rc);
			smem_invalid = true;
		}
	}
	chip->disabled |= smem_invalid ? REASON_SMEM_INVALID : 0;

	if (!smem_invalid) {
		rc = get_cal_data(pdev, chip, vsens_info);
		if (rc < 0) {
			dev_err(dev, "Couldn't get VSENS calibration data rc = %d\n",
						rc);
			goto err;
		}
	}

	chip->reg_base = devm_ioremap(dev, res->start, resource_size(res));
	if (chip->reg_base == NULL) {
		dev_err(dev, "Couldn't ioremap %pa\n", &res->start);
		rc = -EINVAL;
		goto err;
	}

	/* disable the sensors at probe */
	vsens_hw_uninit(chip);

	init_data_voltage->constraints.valid_ops_mask |=
				REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS;
	init_data_voltage->constraints.input_uV =
					init_data_voltage->constraints.max_uV;

	chip->desc_voltage.name = init_data_voltage->constraints.name;
	chip->desc_voltage.ops = &vsens_regulator_ops_voltage;
	chip->desc_voltage.type = REGULATOR_VOLTAGE;
	chip->desc_voltage.owner = THIS_MODULE;

	reg_config_voltage.dev = dev;
	reg_config_voltage.init_data = init_data_voltage;
	reg_config_voltage.driver_data = chip;
	reg_config_voltage.of_node = chip->voltage_node;
	chip->rdev_voltage = regulator_register(&chip->desc_voltage,
						&reg_config_voltage);
	if (IS_ERR(chip->rdev_voltage)) {
		rc = PTR_ERR(chip->rdev_voltage);
		dev_err(dev, "regulator_register (voltage) failed, rc=%d\n",
					rc);
		goto err;
	}

	init_data_corner->constraints.valid_ops_mask |=
				REGULATOR_CHANGE_VOLTAGE;
	init_data_corner->constraints.input_uV
		= init_data_corner->constraints.max_uV;

	chip->desc_corner.name = init_data_corner->constraints.name;
	chip->desc_corner.ops = &vsens_regulator_ops_corner;
	chip->desc_corner.type = REGULATOR_VOLTAGE;
	chip->desc_corner.owner = THIS_MODULE;

	reg_config_corner.dev = dev;
	reg_config_corner.init_data = init_data_corner;
	reg_config_corner.driver_data = chip;
	reg_config_corner.of_node = chip->corner_node;
	chip->rdev_corner = regulator_register(&chip->desc_corner,
					&reg_config_corner);
	if (IS_ERR(chip->rdev_corner)) {
		rc = PTR_ERR(chip->rdev_corner);
		dev_err(dev, "regulator_register (corner) failed, rc=%d\n", rc);
		goto unregister_voltage_regulator;
	}

	chip->dev = dev;
	if (chip->max_irq <= 0) {
		dev_err(dev, "%s: no max irq support\n", chip->name);
	} else {
		snprintf(chip->irq_max_name, 63, "%s-max", chip->name);
		rc = devm_request_threaded_irq(dev, chip->max_irq,
				       NULL, min_max_handler,
				       IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				       chip->irq_max_name, chip);
		if (rc < 0) {
			dev_err(&pdev->dev, "Couldn't register %s irq %d rc = %d\n",
					chip->irq_max_name, chip->max_irq, rc);
			goto unregister_corner_regulator;
		}
	}

	if (chip->min_irq <= 0) {
		dev_err(dev, "%s: no min irq support\n", chip->name);
	} else {
		snprintf(chip->irq_min_name, 63, "%s-min", chip->name);
		rc = devm_request_threaded_irq(dev, chip->min_irq,
				       NULL, min_max_handler,
				       IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				       chip->irq_min_name, chip);
		if (rc < 0) {
			dev_err(dev, "Couldn't register %s irq %d rc = %d\n",
				chip->irq_min_name, chip->min_irq, rc);
			goto unregister_corner_regulator;
		}
	}

	platform_set_drvdata(pdev, chip);

	rc = create_debug_fs_nodes(chip);
	if (rc < 0) {
		dev_err(dev, "Couldn't create debugfs entries rc = %d\n", rc);
		goto unregister_corner_regulator;
	}

	pr_info("%s: Probe Success!\n", chip->name);

	return 0;

unregister_corner_regulator:
	regulator_unregister(chip->rdev_corner);
unregister_voltage_regulator:
	regulator_unregister(chip->rdev_voltage);
err:
	dev_err(dev, "Probe Failed rc = %d\n", rc);
	return rc;
}

static int msm_vsens_remove(struct platform_device *pdev)
{
	unsigned long flags;
	struct vsens_chip *chip = platform_get_drvdata(pdev);

	spin_lock_irqsave(&chip->lock, flags);
	if (!(chip->disabled & REASON_USER)) {
		cpu_pm_unregister_notifier(&chip->nb);
		__vsens_disable(chip, REASON_SUSPEND);
	}
	spin_unlock_irqrestore(&chip->lock, flags);

	if (!(chip->disabled & REASON_USER) && chip->vsens_clk)
		clk_disable_unprepare(chip->vsens_clk);

	regulator_unregister(chip->rdev_corner);
	regulator_unregister(chip->rdev_voltage);
	debugfs_remove_recursive(chip->dir);

	return 0;
}

static int vsens_suspend(struct device *dev)
{
	struct vsens_chip *chip = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	__vsens_disable(chip, REASON_SUSPEND);
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int vsens_resume(struct device *dev)
{
	struct vsens_chip *chip = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	__vsens_enable(chip, REASON_SUSPEND);
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static const struct dev_pm_ops vsens_pm_ops = {
	.suspend	= vsens_suspend,
	.resume		= vsens_resume,
};

static struct platform_driver msm_vsens_driver = {
	.probe	= msm_vsens_probe,
	.remove = msm_vsens_remove,
	.driver	= {
		.name		= "msm-vsens",
		.owner		= THIS_MODULE,
		.of_match_table	= msm_vsens_match_table,
		.pm		= &vsens_pm_ops,
	},
};

static int __init msm_vsens_init(void)
{
	return platform_driver_register(&msm_vsens_driver);
}

static void __exit msm_vsens_exit(void)
{
	return platform_driver_unregister(&msm_vsens_driver);
}

arch_initcall(msm_vsens_init);
module_exit(msm_vsens_exit);

MODULE_DESCRIPTION("MSM VSENS DRIVER");
MODULE_LICENSE("GPL v2");
