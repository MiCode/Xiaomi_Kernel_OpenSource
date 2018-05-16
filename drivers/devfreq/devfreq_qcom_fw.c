// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/devfreq.h>
#include <linux/pm_opp.h>

#define INIT_HZ				300000000UL
#define XO_HZ				19200000UL
#define FTBL_MAX_ENTRIES		40U
#define FTBL_ROW_SIZE			4

#define SRC_MASK	GENMASK(31, 30)
#define SRC_SHIFT	30
#define MULT_MASK	GENMASK(7, 0)

struct devfreq_qcom_fw {
	void __iomem *perf_base;
	struct devfreq_dev_profile dp;
	struct list_head voters;
	struct list_head voter;
	unsigned int index;
};

static DEFINE_SPINLOCK(voter_lock);

static int devfreq_qcom_fw_target(struct device *dev, unsigned long *freq,
				  u32 flags)
{
	struct devfreq_qcom_fw *d = dev_get_drvdata(dev), *pd, *v;
	struct devfreq_dev_profile *p = &d->dp;
	unsigned int index;
	unsigned long lflags;
	struct dev_pm_opp *opp;
	void __iomem *perf_base = d->perf_base;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (!IS_ERR(opp))
		dev_pm_opp_put(opp);
	else
		return PTR_ERR(opp);

	for (index = 0; index < p->max_state; index++)
		if (p->freq_table[index] == *freq)
			break;

	if (index >= p->max_state) {
		dev_err(dev, "Unable to find index for freq (%lu)!\n", *freq);
		return -EINVAL;
	}

	d->index = index;

	spin_lock_irqsave(&voter_lock, lflags);
	/* Voter */
	if (!perf_base) {
		pd = dev_get_drvdata(dev->parent);
		list_for_each_entry(v, &pd->voters, voter)
			index = max(index, v->index);
		perf_base = pd->perf_base;
	}

	writel_relaxed(index, perf_base);
	spin_unlock_irqrestore(&voter_lock, lflags);

	return 0;
}

static int devfreq_qcom_fw_get_cur_freq(struct device *dev,
						 unsigned long *freq)
{
	struct devfreq_qcom_fw *d = dev_get_drvdata(dev);
	struct devfreq_dev_profile *p = &d->dp;
	unsigned int index;

	/* Voter */
	if (!d->perf_base) {
		index = d->index;
	} else {
		index = readl_relaxed(d->perf_base);
		index = min(index, p->max_state - 1);
	}
	*freq = p->freq_table[index];

	return 0;
}

static int devfreq_qcom_populate_opp(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 data, src, mult, i;
	unsigned long freq, prev_freq;
	struct resource *res;
	void __iomem *ftbl_base;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ftbl-base");
	if (!res) {
		dev_err(dev, "Unable to find ftbl-base!\n");
		return -EINVAL;
	}

	ftbl_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!ftbl_base) {
		dev_err(dev, "Unable to map ftbl-base\n");
		return -ENOMEM;
	}

	for (i = 0; i < FTBL_MAX_ENTRIES; i++) {
		data = readl_relaxed(ftbl_base + i * FTBL_ROW_SIZE);
		src = ((data & SRC_MASK) >> SRC_SHIFT);
		mult = (data & MULT_MASK);
		freq = src ? XO_HZ * mult : INIT_HZ;

		/*
		 * Two of the same frequencies with the same core counts means
		 * end of table.
		 */
		if (i > 0 && prev_freq == freq)
			break;

		dev_pm_opp_add(&pdev->dev, freq, 0);

		prev_freq = freq;
	}

	devm_iounmap(dev, ftbl_base);

	return 0;
}

static int devfreq_qcom_init_hw(struct platform_device *pdev)
{
	struct devfreq_qcom_fw *d;
	struct resource *res;
	struct device *dev = &pdev->dev;
	int ret = 0;
	void __iomem *en_base;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "en-base");
	if (!res) {
		dev_err(dev, "Unable to find en-base!\n");
		return -EINVAL;
	}

	en_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!en_base) {
		dev_err(dev, "Unable to map en-base\n");
		return -ENOMEM;
	}

	/* Firmware should be enabled state to proceed */
	if (!(readl_relaxed(en_base) & 1)) {
		dev_err(dev, "Firmware not enabled\n");
		return -ENODEV;
	}

	devm_iounmap(dev, en_base);

	ret = devfreq_qcom_populate_opp(pdev);
	if (ret) {
		dev_err(dev, "Failed to read FTBL\n");
		return ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "perf-base");
	if (!res) {
		dev_err(dev, "Unable to find perf-base!\n");
		ret = -EINVAL;
		goto out;
	}

	d->perf_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!d->perf_base) {
		dev_err(dev, "Unable to map perf-base\n");
		ret = -ENOMEM;
		goto out;
	}

	INIT_LIST_HEAD(&d->voters);
	dev_set_drvdata(dev, d);

out:
	if (ret)
		dev_pm_opp_remove_table(dev);
	return ret;
}

static int devfreq_qcom_copy_opp(struct device *src_dev, struct device *dst_dev)
{
	unsigned long freq;
	int i, cnt, ret = 0;
	struct dev_pm_opp *opp;

	if (!src_dev)
		return -ENODEV;

	cnt = dev_pm_opp_get_opp_count(src_dev);
	if (!cnt)
		return -EINVAL;

	for (i = 0, freq = 0; i < cnt; i++, freq++) {
		opp = dev_pm_opp_find_freq_ceil(src_dev, &freq);
		if (IS_ERR(opp)) {
			ret = -EINVAL;
			break;
		}
		dev_pm_opp_put(opp);

		ret = dev_pm_opp_add(dst_dev, freq, 0);
		if (ret)
			break;
	}

	if (ret)
		dev_pm_opp_remove_table(dst_dev);
	return ret;
}

static int devfreq_qcom_init_voter(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *par_dev = dev->parent;
	struct devfreq_qcom_fw *d, *pd = dev_get_drvdata(par_dev);
	int ret = 0;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	ret = devfreq_qcom_copy_opp(dev->parent, dev);
	if (ret) {
		dev_err(dev, "Failed to copy parent OPPs\n");
		return ret;
	}

	list_add(&d->voter, &pd->voters);
	dev_set_drvdata(dev, d);

	return 0;
}

static int devfreq_qcom_fw_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct devfreq_qcom_fw *d;
	struct devfreq_dev_profile *p;
	struct devfreq *df;

	if (!of_device_get_match_data(dev))
		ret = devfreq_qcom_init_voter(pdev);
	else
		ret = devfreq_qcom_init_hw(pdev);
	if (ret) {
		dev_err(dev, "Unable to probe device!\n");
		return ret;
	}

	/*
	 * If device has voter children, do no register directly with devfreq
	 */
	if (of_get_available_child_count(dev->of_node)) {
		of_platform_populate(dev->of_node, NULL, NULL, dev);
		dev_info(dev, "Devfreq QCOM Firmware parent dev inited.\n");
		return 0;
	}

	d = dev_get_drvdata(dev);
	p = &d->dp;
	p->polling_ms = 50;
	p->target = devfreq_qcom_fw_target;
	p->get_cur_freq = devfreq_qcom_fw_get_cur_freq;

	df = devm_devfreq_add_device(dev, p, "performance", NULL);
	if (IS_ERR(df)) {
		dev_err(dev, "Unable to register Devfreq QCOM Firmware dev!\n");
		return PTR_ERR(df);
	}

	dev_info(dev, "Devfreq QCOM Firmware dev registered.\n");

	return 0;
}

static const struct of_device_id match_table[] = {
	{ .compatible = "qcom,devfreq-fw", .data = (void *) 1 },
	{ .compatible = "qcom,devfreq-fw-voter", .data = (void *) 0 },
	{}
};

static struct platform_driver devfreq_qcom_fw_driver = {
	.probe = devfreq_qcom_fw_driver_probe,
	.driver = {
		.name = "devfreq-qcom-fw",
		.of_match_table = match_table,
	},
};

static int __init devfreq_qcom_fw_init(void)
{
	return platform_driver_register(&devfreq_qcom_fw_driver);
}
subsys_initcall(devfreq_qcom_fw_init);

static void __exit devfreq_qcom_fw_exit(void)
{
	platform_driver_unregister(&devfreq_qcom_fw_driver);
}
module_exit(devfreq_qcom_fw_exit);

MODULE_DESCRIPTION("Devfreq QCOM Firmware");
MODULE_LICENSE("GPL v2");
