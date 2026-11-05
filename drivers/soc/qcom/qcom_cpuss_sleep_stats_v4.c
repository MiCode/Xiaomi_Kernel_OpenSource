// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define NAME_LENGTH 0x12
#define REGS_MAX 3

struct regs_config {
	void __iomem *base;
	u32 offset;
	char name[NAME_LENGTH];
	u32 residency_lo_hi_distance;
};

struct stats_config {
	struct regs_config count_regs[REGS_MAX];
	struct regs_config residency_regs[REGS_MAX];
};

struct qcom_stats_prvdata {
	struct platform_device *pdev;
	void __iomem *base;
	struct stats_config *offset;
	struct dentry *rootdir;
};

static int qcom_stats_count_show(struct seq_file *s, void *d)
{
	struct regs_config *reg = s->private;
	u32 val;

	val = readl_relaxed(reg->base + reg->offset);
	seq_printf(s, "%u\n", val);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(qcom_stats_count);

static int qcom_stats_residency_show(struct seq_file *s, void *d)
{
	struct regs_config *reg = s->private;
	u64 val;

	val = (u64)readl_relaxed(reg->base + reg->offset
				+ reg->residency_lo_hi_distance) << 32;
	val += readl_relaxed(reg->base + reg->offset);
	seq_printf(s, "%llu\n", val);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(qcom_stats_residency);

static void qcom_create_count_file(struct qcom_stats_prvdata *pdata)
{
	int i;
	struct stats_config *config = pdata->offset;

	for (i = 0; i < ARRAY_SIZE(config->count_regs); i++) {
		pdata->offset->count_regs[i].base = pdata->base;
		debugfs_create_file(pdata->offset->count_regs[i].name, 0400, pdata->rootdir,
				(void *)&pdata->offset->count_regs[i],
				&qcom_stats_count_fops);
	}
}

static void qcom_create_resindency_file(struct qcom_stats_prvdata *pdata)
{
	int i;
	struct stats_config *config = pdata->offset;

	for (i = 0; i < ARRAY_SIZE(config->residency_regs); i++) {
		pdata->offset->residency_regs[i].base = pdata->base;
		debugfs_create_file(pdata->offset->residency_regs[i].name, 0400, pdata->rootdir,
				(void *)&pdata->offset->residency_regs[i],
				&qcom_stats_residency_fops);
	}
}

static int qcom_cpuss_sleep_stats_v4_probe(struct platform_device *pdev)
{
	struct qcom_stats_prvdata *pdata;

	pdata = devm_kzalloc(&pdev->dev, sizeof(struct qcom_stats_prvdata),
			      GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->base = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(pdata->base))
		return PTR_ERR(pdata->base);

	pdata->rootdir = debugfs_create_dir("qcom_cpuss_sleep_stats", NULL);
	pdata->pdev = pdev;

	pdata->offset = (struct stats_config *)of_device_get_match_data(&pdev->dev);
	if (!pdata->offset)
		return -EINVAL;

	qcom_create_count_file(pdata);
	qcom_create_resindency_file(pdata);

	platform_set_drvdata(pdev, pdata->rootdir);

	return 0;
}

static int qcom_cpuss_sleep_stats_remove(struct platform_device *pdev)
{
	struct dentry *root = platform_get_drvdata(pdev);

	debugfs_remove_recursive(root);

	return 0;
}

struct stats_config qcom_cpuss_cntr_offsets = {
	.count_regs = {
		{
			.offset = 0x1220,
			.name = "CL0_CL5_Count",
		},
		{
			.offset = 0x1224,
			.name = "CL1_CL5_Count",
		},
		{
			.offset = 0x1018,
			.name = "SS3_Count",
		},
	},
	.residency_regs = {
		{
			.offset = 0x1320,
			.name = "CL0_CL5_Residency",
			.residency_lo_hi_distance = 0x10,
		},
		{
			.offset = 0x1324,
			.name = "CL1_CL5_Residency",
			.residency_lo_hi_distance = 0x10,
		},
		{
			.offset = 0x1118,
			.name = "SS3_Residency",
			.residency_lo_hi_distance = 0x4,
		},
	},
};

static const struct of_device_id qcom_cpuss_stats_v4_table[] = {
	{ .compatible = "qcom,cpuss-sleep-stats-v4", .data = &qcom_cpuss_cntr_offsets },
	{ },
};

static struct platform_driver qcom_cpuss_sleep_stats_v4 = {
	.probe = qcom_cpuss_sleep_stats_v4_probe,
	.remove = qcom_cpuss_sleep_stats_remove,
	.driver	= {
		.name = "qcom_cpuss_sleep_stats_v4",
		.of_match_table	= qcom_cpuss_stats_v4_table,
	},
};

module_platform_driver(qcom_cpuss_sleep_stats_v4);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. (QTI) CPUSS sleep stats v4 driver");
MODULE_LICENSE("GPL");
