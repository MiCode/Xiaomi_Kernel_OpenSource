// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "mmdvfs_wrapper.h"

#define MAX_OPP_LEVEL (5)

static const char * const clk_names[] = {
	"mm",
	"cam",
	"img",
	"venc",
	"vdec",
	"ipe",
	"dpe",
	"ccu",
};

struct clk_data {
	struct device *dev;
	struct regulator *reg;
	u32 freqs[MAX_OPP_LEVEL];
};

struct wrapper_data {
	struct clk_data *clks;
	const char *reg_name;
	const u32 voltages[MAX_OPP_LEVEL];
	const u32 num_opp_level;
};

static struct clk_data clk_data_mt6779[] = {
	[CLK_MM] = {
		.freqs = {315, 450, 606},
	},
	[CLK_CAM] = {
		.freqs = {315, 416, 560},
	},
	[CLK_IMG] = {
		.freqs = {315, 416, 606},
	},
	[CLK_VENC] = {
		.freqs = {364, 450, 630},
	},
	[CLK_VDEC] = {
		.freqs = {312, 416, 624},
	},
	[CLK_CCU] = {
		.freqs = {315, 416, 560},
	},
	[CLK_IPE] = {
		.freqs = {315, 416, 546},
	},
	[CLK_DPE] = {
		.freqs = {364, 450, 546},
	},
};

static struct wrapper_data wrapper_data_mt6779 = {
	.clks = clk_data_mt6779,
	.voltages = {650000, 725000, 825000},
	.reg_name = "dvfsrc-vcore",
	.num_opp_level = 3,
};

static struct wrapper_data *mmdvfs_wrapper;

/**
 * mmdvfs_wrapper_set_freq - set frequency
 * @clk_id: clk id to set frequency
 * @freq: frequency reguirement in MHz
 *
 * Returns 0 on success, or an appropriate error code otherwise
 */
s32 mmdvfs_wrapper_set_freq(u32 clk_id, u32 freq)
{
	u32 i;
	s32 low_volt, high_volt, ret = 0;

	if (clk_id >= CLK_MAX_NUM)
		return -EINVAL;
	for (i = 0; i < mmdvfs_wrapper->num_opp_level; i++)
		if (freq <= mmdvfs_wrapper->clks[clk_id].freqs[i])
			break;

	if (i == mmdvfs_wrapper->num_opp_level && i)
		i = mmdvfs_wrapper->num_opp_level - 1;
	high_volt = mmdvfs_wrapper->voltages[mmdvfs_wrapper->num_opp_level - 1];
	low_volt = mmdvfs_wrapper->voltages[i];
	if (mmdvfs_wrapper->clks[clk_id].reg)
		ret = regulator_set_voltage(mmdvfs_wrapper->clks[clk_id].reg,
			low_volt, high_volt);

	return ret;
}
EXPORT_SYMBOL_GPL(mmdvfs_wrapper_set_freq);

/**
 * mmdvfs_wrapper_get_freq_steps - get available frequency number and array
 * @clk_id: clk id to get frequency information
 * @freq_steps: available frequency array and maximum size is MAX_FREQ_STEP
 * The order of freq steps is from high to low
 * @step_size: size of available items in freq_steps
 *
 * Returns 0 on success, or an appropriate error code otherwise
 */
s32 mmdvfs_wrapper_get_freq_steps(
	u32 clk_id, u64 *freq_steps, u32 *step_size)
{
	struct clk_data *clks = mmdvfs_wrapper->clks;
	u32 opp_size = mmdvfs_wrapper->num_opp_level;
	u32 i;

	if (!freq_steps || !step_size)
		return -EINVAL;

	*step_size = opp_size;
	for (i = 0; i < opp_size; i++)
		freq_steps[i] = clks[clk_id].freqs[opp_size-1-i];

	return 0;
}
EXPORT_SYMBOL_GPL(mmdvfs_wrapper_get_freq_steps);

/**
 * mmdvfs_qos_get_freq - get current frequency
 * @clk_id: clk id to get current frequency
 *
 * Returns frequency in MHz, or 0 if something wrong
 */
u64 mmdvfs_qos_get_freq(u32 clk_id)
{
	struct regulator *reg;
	int voltage = 0, i;

	if (clk_id >= CLK_MAX_NUM)
		return 0;

	reg = mmdvfs_wrapper->clks[clk_id].reg;
	if (reg)
		voltage = regulator_get_voltage(reg);

	for (i = 0; i < ARRAY_SIZE(mmdvfs_wrapper->voltages); i++) {
		if (mmdvfs_wrapper->voltages[i] == voltage)
			break;
	}

	if (i == ARRAY_SIZE(mmdvfs_wrapper->voltages))
		return 0;

	return mmdvfs_wrapper->clks[clk_id].freqs[i];
}
EXPORT_SYMBOL_GPL(mmdvfs_qos_get_freq);


static const struct of_device_id of_mmdvfs_wrapper_match_tbl[] = {
	{
		.compatible = "mediatek,mt6779-mmdvfs-wrapper",
		.data = &wrapper_data_mt6779,
	},
	{}
};

static int mmdvfs_wrapper_probe(struct platform_device *pdev)
{
	s32 ret, i;
	const char *name;

	mmdvfs_wrapper =
		(struct wrapper_data *)of_device_get_match_data(&pdev->dev);
	ret = of_property_read_string(
			pdev->dev.of_node, "mediatek,name", &name);
	if (ret) {
		dev_notice(&pdev->dev, "name is not found in dts\n");
		return -EINVAL;
	}
	for (i = 0; i < ARRAY_SIZE(clk_names); i++) {
		if (!strcmp(clk_names[i], name))
			break;
	}
	if (i != ARRAY_SIZE(clk_names)) {
		mmdvfs_wrapper->clks[i].dev = &pdev->dev;
		mmdvfs_wrapper->clks[i].reg = devm_regulator_get(
			&pdev->dev, mmdvfs_wrapper->reg_name);
	}

	/* Todo: Remove it when clk/vcore's initial opp is lowest */
	mmdvfs_wrapper_set_freq(CLK_MM, 0);

	return 0;
}

static struct platform_driver mmdvfs_wrapper_drv = {
	.probe = mmdvfs_wrapper_probe,
	.driver = {
		.name = "mtk-mmdvfs-wrapper",
		.owner = THIS_MODULE,
		.of_match_table = of_mmdvfs_wrapper_match_tbl,
	},
};

static int __init mtk_mmdvfs_wrapper_init(void)
{
	s32 status;

	status = platform_driver_register(&mmdvfs_wrapper_drv);
	if (status) {
		pr_notice(
			"Failed to register MMDVFS wrapper driver(%d)\n",
			status);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_mmdvfs_wrapper_exit(void)
{
	platform_driver_unregister(&mmdvfs_wrapper_drv);
}

module_init(mtk_mmdvfs_wrapper_init);
module_exit(mtk_mmdvfs_wrapper_exit);


static u32 test_freq;
int set_test_freq(const char *val, const struct kernel_param *kp)
{
	u32 clk_id, freq;
	s32 ret;

	ret = sscanf(val, "%u %u", &clk_id, &freq);
	pr_info("%s: ret:%d input:%s", __func__, ret, val);
	if (ret != 2 || clk_id >= CLK_MAX_NUM)
		return -EINVAL;

	ret = mmdvfs_wrapper_set_freq(clk_id, freq);
	return ret;
}

int get_test_freq(char *buf, const struct kernel_param *kp)
{
	u32 i, clk_id, step_size = 0;
	u64 steps[5];
	s32 ret;
	int length = 0;

	for (clk_id = 0; clk_id < CLK_MAX_NUM; clk_id++) {
		ret = mmdvfs_wrapper_get_freq_steps(clk_id, steps, &step_size);
		if (ret)
			return -EINVAL;
		length += snprintf(buf + length, PAGE_SIZE - length,
			"clk_id=%d step_size=%d\n", clk_id, step_size);

		for (i = 0; i < step_size; i++) {
			if (i == 0)
				length += snprintf(buf + length,
					PAGE_SIZE - length, "(");

			if (i == step_size - 1)
				length += snprintf(buf + length,
					PAGE_SIZE - length, "%llu)\n",
					steps[i]);
			else
				length += snprintf(buf + length,
					PAGE_SIZE - length, "%llu,", steps[i]);
		}
	}

	buf[length] = '\0';

	return length;
}

static struct kernel_param_ops test_freq_ops = {
	.set = set_test_freq,
	.get = get_test_freq,
};
module_param_cb(test_freq, &test_freq_ops, &test_freq, 0644);

MODULE_DESCRIPTION("MTK MMDVFS wrapper driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
