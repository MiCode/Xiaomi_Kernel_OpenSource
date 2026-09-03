// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Spreadtrum Communications Inc.

#include <linux/gpio/consumer.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/debugfs.h>

/* PMIC global control registers definition */
#define SC2730_MODULE_EN0		0x1808
#define SC2730_CLK_EN0			0x1810
#define UMP9620_MODULE_EN0		0x2008
#define UMP9620_CLK_EN0			0x2010
#define SC2720_MODULE_EN0		0xc08
#define SC2720_CLK_EN0			0xc10

/* SMPL function */
#define SC2720_GLOBAL_SMPL_CTRL0	0x24c
#define SC2720_GLOBAL_SMPL_CTRL1	0x250
#define SC2721_GLOBAL_SMPL_CTRL0	0x300
#define SC2721_GLOBAL_SMPL_CTRL1	0x304
#define SC2730_GLOBAL_SMPL_CTRL0	0x3dc
#define SC2730_GLOBAL_SMPL_CTRL1	0x3e0
/* SMPL crtl0 */
#define SC2730_GLOBAL_SMPL_TIMER_MASK	GENMASK(15, 13)
#define SC2730_GLOBAL_SMPL_TIMER_SHIFT	13
#define SC2730_GLOBAL_SMPL_ENABLE_MASK	GENMASK(12, 0)
#define SC2730_GLOBAL_SMPL_ENABLE_SHIFT	0
/* SMPL ctrl1 */
#define SC2730_SMPL_PWR_ON_FLAG_MASK		GENMASK(15, 15)
#define SC2730_SMPL_PWR_ON_FLAG_SHIFT		15
#define SC2730_SMPL_MODE_ACK_FLAG_MASK		GENMASK(14, 14)
#define SC2730_SMPL_MODE_ACK_FLAG_SHIFT		14
#define SC2730_SMPL_PWR_ON_FLAG_CLR_MASK	GENMASK(13, 13)
#define SC2730_SMPL_PWR_ON_FLAG_CLR_SHIFT	13
#define	SC2730_SMPL_MODE_ACK_FLAG_CLR_MASK	GENMASK(12, 12)
#define SC2730_SMPL_MODE_ACK_FLAG_CLR_SHIFT	12
#define SC2730_SMPL_PWR_ON_SET_MASK		GENMASK(11, 11)
#define SC2730_SMPL_PWR_ON_SET_SHIFT		11
#define SC2730_SMPL_ENABLE_FLAG_MASK		GENMASK(0, 0)
#define SC2730_SMPL_ENABLE_FLAG_SHIF		0
/* const data define*/
#define SC2730_GLOBAL_SMPL_TIMER_STEP_MS	250
#define SC2730_GLOBAL_SMPL_TIMER_MIN_MS		250
#define SC2730_GLOBAL_SMPL_TIMER_MAX_MS		2000
#define SC2730_GLOBAL_SMPL_ENABLE		0x1935
#define SC2730_GLOBAL_SMPL_DISABLE		0x0
#define UMP9620_GLOBAL_SMPL_TIMER_STEP_MS	150
#define UMP9620_GLOBAL_SMPL_TIMER_MIN_MS	150
#define UMP9620_GLOBAL_SMPL_TIMER_MAX_MS	1000

struct sprd_pmic_smpl_data {
	struct regmap *regmap;
	struct device *dev;
	struct mutex lock;
	u32 base;
	u32 timer_time;
	int enable;
	const struct sprd_pmic_smpl_variant_data *pdata;
	struct dentry *monitor_dir;
};

struct sprd_pmic_smpl_data *data;

struct sprd_pmic_smpl_variant_data {
	u32 module_en;
	u32 clk_en;
	u32 smpl_ctrl0;
	u32 smpl_ctrl1;
	u32 timer_step;
	u32 timer_min;
	u32 timer_max;
};

static const struct sprd_pmic_smpl_variant_data sc2720_info = {
	.module_en = SC2720_MODULE_EN0,
	.clk_en = SC2720_CLK_EN0,
	.smpl_ctrl0 = SC2720_GLOBAL_SMPL_CTRL0,
	.smpl_ctrl1 = SC2720_GLOBAL_SMPL_CTRL1,
	.timer_step = SC2730_GLOBAL_SMPL_TIMER_STEP_MS,
	.timer_min = SC2730_GLOBAL_SMPL_TIMER_MIN_MS,
	.timer_max = SC2730_GLOBAL_SMPL_TIMER_MAX_MS,
};

static const struct sprd_pmic_smpl_variant_data sc2721_info = {
	.module_en = SC2720_MODULE_EN0,
	.clk_en = SC2720_CLK_EN0,
	.smpl_ctrl0 = SC2721_GLOBAL_SMPL_CTRL0,
	.smpl_ctrl1 = SC2721_GLOBAL_SMPL_CTRL1,
	.timer_step = SC2730_GLOBAL_SMPL_TIMER_STEP_MS,
	.timer_min = SC2730_GLOBAL_SMPL_TIMER_MIN_MS,
	.timer_max = SC2730_GLOBAL_SMPL_TIMER_MAX_MS,
};

static const struct sprd_pmic_smpl_variant_data sc2730_info = {
	.module_en = SC2730_MODULE_EN0,
	.clk_en = SC2730_CLK_EN0,
	.smpl_ctrl0 = SC2730_GLOBAL_SMPL_CTRL0,
	.smpl_ctrl1 = SC2730_GLOBAL_SMPL_CTRL1,
	.timer_step = SC2730_GLOBAL_SMPL_TIMER_STEP_MS,
	.timer_min = SC2730_GLOBAL_SMPL_TIMER_MIN_MS,
	.timer_max = SC2730_GLOBAL_SMPL_TIMER_MAX_MS,
};

static const struct sprd_pmic_smpl_variant_data ump9620_info = {
	.module_en = UMP9620_MODULE_EN0,
	.clk_en = UMP9620_CLK_EN0,
	.smpl_ctrl0 = SC2730_GLOBAL_SMPL_CTRL0,
	.smpl_ctrl1 = SC2730_GLOBAL_SMPL_CTRL1,
	.timer_step = UMP9620_GLOBAL_SMPL_TIMER_STEP_MS,
	.timer_min = UMP9620_GLOBAL_SMPL_TIMER_MIN_MS,
	.timer_max = UMP9620_GLOBAL_SMPL_TIMER_MAX_MS,
};

static int sprd_pmic_smpl_get_reg(u32 reg, u32 *reg_val)
{
	int ret = 0;

	ret = regmap_read(data->regmap, data->base + reg, reg_val);
	if (ret)
		dev_info(data->dev, "failed to get reg(0x%04x), ret = %d\n", reg, ret);
	return ret;
}

static void sprd_pmic_smpl_enable(struct sprd_pmic_smpl_data *data, int enable)
{
	const struct sprd_pmic_smpl_variant_data *pdata = data->pdata;
	int ret = 0;

	if (!pdata->smpl_ctrl0) {
		dev_err(data->dev, " Not support smpl\n");
		return;
	}

	ret = regmap_update_bits(data->regmap, data->base + pdata->smpl_ctrl0,
				 SC2730_GLOBAL_SMPL_ENABLE_MASK, enable);
	if (ret)
		dev_err(data->dev, " failed to enable smpl\n");
}

static void sprd_pmic_smpl_set_timer(struct sprd_pmic_smpl_data *data, int ms)
{
	const struct sprd_pmic_smpl_variant_data *pdata = data->pdata;
	int ret = 0;
	u32 reg_val;

	if (!pdata->smpl_ctrl0) {
		dev_info(data->dev, "Not support smpl\n");
		return;
	}

	if (ms < pdata->timer_min)
		ms = pdata->timer_min;
	else if (ms > pdata->timer_max)
		ms = pdata->timer_max;

	reg_val = (ms - pdata->timer_min) / pdata->timer_step;

	ret = regmap_update_bits(data->regmap, data->base + pdata->smpl_ctrl0,
				 SC2730_GLOBAL_SMPL_TIMER_MASK,
				 reg_val << SC2730_GLOBAL_SMPL_TIMER_SHIFT);
	if (ret)
		dev_err(data->dev, "failed to set timer value 0x%02x(%dms)\n", reg_val, ms);
}

static int info_proc_show(struct seq_file *m, void *v)
{
	int ret = 0;
	u32 value = 0;

	ret = sprd_pmic_smpl_get_reg(data->pdata->smpl_ctrl0, &value);
	if (ret) {
		dev_err(data->dev, "read CTRL0 is error\n");
		return ret;
	}
	seq_printf(m, "smpl_ctrl0  %x\n", value);

	ret = sprd_pmic_smpl_get_reg(data->pdata->smpl_ctrl1, &value);
	if (ret) {
		dev_err(data->dev, "read CTRL1 is error\n");
		return ret;
	}
	seq_printf(m, "smpl_ctrl1  %x\n", value);
	return 0;
}

static ssize_t info_proc_write(struct file *filep,
			       const char __user *buf,
			       size_t len, loff_t *ppos)
{
	int ret;
	unsigned int value;

	ret = kstrtouint_from_user(buf, len, 0, &value);
	if (ret) {
		dev_err(data->dev, "%s,  0x%x\n", __func__, value);
		return -EINVAL;
	}

	ret = regmap_update_bits(data->regmap,
				 data->base + data->pdata->smpl_ctrl1,
				 SC2730_SMPL_PWR_ON_FLAG_CLR_MASK |
				 SC2730_SMPL_MODE_ACK_FLAG_CLR_MASK,
				 SC2730_SMPL_PWR_ON_FLAG_CLR_MASK |
				 SC2730_SMPL_MODE_ACK_FLAG_CLR_MASK);
	if (ret) {
		dev_err(data->dev, "failed to clr smpl\n");
		return ret;
	}
	ret = regmap_update_bits(data->regmap,
				 data->base + data->pdata->smpl_ctrl0,
				 SC2730_GLOBAL_SMPL_TIMER_MASK |
				 SC2730_GLOBAL_SMPL_ENABLE_MASK,
				 value);
	if (ret) {
		dev_err(data->dev, "failed to set smpl\n");
		return ret;
	}

	return len;
}

static int info_proc_open(struct inode *inode, struct file *file)
{
	single_open(file, info_proc_show, NULL);
	return 0;
}

static const struct file_operations info_proc_fops = {
	.open    = info_proc_open,
	.read    = seq_read,
	.write   = info_proc_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int sprd_pmic_smpl_probe(struct platform_device *pdev)
{
	int ret;

	data =  devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "failed to get smpl data\n");
		return -ENOMEM;
	}
	data->pdata = of_device_get_match_data(&pdev->dev);
	if (!data->pdata) {
		dev_err(&pdev->dev, "no matching driver data found\n");
		return -ENODEV;
	}

	data->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!data->regmap) {
		dev_err(&pdev->dev, "failed to get regmap: %d\n", __LINE__);
		return -ENODEV;
	}

	ret = device_property_read_u32(&pdev->dev, "reg", &data->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get smpl address\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev, "sprd,smpl-timer-threshold",
				       &data->timer_time);
	if (ret) {
		dev_err(&pdev->dev, "failed to get sprd,smpl-timer-threshold\n");
		data->timer_time = data->pdata->timer_min;
	}

	mutex_init(&data->lock);

	data->dev = &pdev->dev;
	platform_set_drvdata(pdev, data);
	data->monitor_dir = debugfs_create_dir("pmic_smpl", NULL);
	if (!data->monitor_dir) {
		pr_err("pub_monitor creat dir error\n");
		return -ENOMEM;
	}

	debugfs_create_file("enable", 0664, data->monitor_dir, NULL, &info_proc_fops);

	sprd_pmic_smpl_set_timer(data, data->timer_time);
	sprd_pmic_smpl_enable(data, SC2730_GLOBAL_SMPL_ENABLE);

	return 0;
}

static const struct of_device_id sc27xx_smpl_of_match[] = {
	{.compatible = "sprd,sc2720-smpl", .data = &sc2720_info},
	{.compatible = "sprd,sc2721-smpl", .data = &sc2721_info},
	{.compatible = "sprd,sc2730-smpl", .data = &sc2730_info},
	{.compatible = "sprd,ump9620-smpl", .data = &ump9620_info},
	{}
};

static struct platform_driver sprd_pmic_smpl = {
	.probe =  sprd_pmic_smpl_probe,
	.driver = {
		.name = "sprd-pmic-smpl",
		.of_match_table = sc27xx_smpl_of_match,
	}
};

module_platform_driver(sprd_pmic_smpl);

MODULE_DESCRIPTION("Unisoc SC27XX PMICs Sudden Momentary Power Loss Driver");
MODULE_LICENSE("GPL v2");

