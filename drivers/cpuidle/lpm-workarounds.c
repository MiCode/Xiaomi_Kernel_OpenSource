/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/rpm-smd-regulator.h>

static struct regulator *lpm_cx_reg;
static struct work_struct dummy_vote_work;
static struct workqueue_struct *lpm_wa_wq;
static bool lpm_wa_cx_turbo_unvote;

/* While exiting from RPM assisted power collapse on some targets like MSM8939
 * the CX is bumped to turbo mode by RPM. To reduce the power impact, APSS
 * low power driver need to remove the CX turbo vote.
 */
static void send_dummy_cx_vote(struct work_struct *w)
{
	if (lpm_cx_reg) {
		regulator_set_voltage(lpm_cx_reg,
			RPM_REGULATOR_CORNER_SUPER_TURBO,
			RPM_REGULATOR_CORNER_SUPER_TURBO);

		regulator_set_voltage(lpm_cx_reg,
			RPM_REGULATOR_CORNER_NONE,
			RPM_REGULATOR_CORNER_SUPER_TURBO);
	}
}

/*
 * lpm_wa_cx_unvote_send(): Unvote for CX turbo mode
 */
void lpm_wa_cx_unvote_send(void)
{
	if (lpm_wa_cx_turbo_unvote)
		queue_work(lpm_wa_wq, &dummy_vote_work);
}
EXPORT_SYMBOL(lpm_wa_cx_unvote_send);

static int lpm_wa_cx_unvote_init(struct platform_device *pdev)
{
	int ret = 0;

	lpm_cx_reg = devm_regulator_get(&pdev->dev, "lpm-cx");
	if (IS_ERR(lpm_cx_reg)) {
		ret = PTR_ERR(lpm_cx_reg);
		if (ret != -EPROBE_DEFER)
			pr_err("Unable to get the CX regulator\n");
		return ret;
	}

	INIT_WORK(&dummy_vote_work, send_dummy_cx_vote);

	lpm_wa_wq = alloc_workqueue("lpm-wa",
				WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);

	return ret;
}

static int lpm_wa_cx_unvote_exit(void)
{
	if (lpm_wa_wq)
		destroy_workqueue(lpm_wa_wq);

	return 0;
}

static int lpm_wa_probe(struct platform_device *pdev)
{
	int ret = 0;

	lpm_wa_cx_turbo_unvote = of_property_read_bool(pdev->dev.of_node,
					"qcom,lpm-wa-cx-turbo-unvote");
	if (lpm_wa_cx_turbo_unvote) {
		ret = lpm_wa_cx_unvote_init(pdev);
		if (ret) {
			pr_err("%s: Failed to initialize lpm_wa_cx_unvote (%d)\n",
				__func__, ret);
			return ret;
		}
	}

	return ret;
}

static int lpm_wa_remove(struct platform_device *pdev)
{
	int ret = 0;
	if (lpm_wa_cx_turbo_unvote)
		ret = lpm_wa_cx_unvote_exit();

	return ret;
}

static struct of_device_id lpm_wa_mtch_tbl[] = {
	{.compatible = "qcom,lpm-workarounds"},
	{},
};

static struct platform_driver lpm_wa_driver = {
	.probe = lpm_wa_probe,
	.remove = lpm_wa_remove,
	.driver = {
		.name = "lpm-workarounds",
		.owner = THIS_MODULE,
		.of_match_table = lpm_wa_mtch_tbl,
	},
};

static int __init lpm_wa_module_init(void)
{
	int ret;
	ret = platform_driver_register(&lpm_wa_driver);
	if (ret)
		pr_info("Error registering %s\n", lpm_wa_driver.driver.name);

	return ret;
}
late_initcall(lpm_wa_module_init);
