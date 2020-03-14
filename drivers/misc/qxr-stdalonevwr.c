// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpmh-regulator.h>

struct qxr_stdalonevwr {
	struct platform_device *pdev;
	struct regulator *reg_imu;
	int ndi_5v_en;
	bool initDone;
};

static struct qxr_stdalonevwr *pdata;

static int qxr_stdalonevwr_allocate_res(void)
{
	int rc = -EINVAL;
	bool gpioEnabled = false;

	if (pdata->initDone) {
		pr_debug("%s init is done already\n", __func__);
		return 0;
	}
	/* Invensense 3.3 PowerRail */
	pdata->reg_imu = devm_regulator_get(&pdata->pdev->dev, "pm8150a_l11");
	if (!IS_ERR(pdata->reg_imu)) {
		regulator_set_load(pdata->reg_imu, 600000);
		rc = regulator_enable(pdata->reg_imu);
		if (rc < 0) {
			pr_err("%s IMU rail pm8150a_l11 failed\n", __func__);
			devm_regulator_put(pdata->reg_imu);
		}
	}

	if (gpio_is_valid(pdata->ndi_5v_en)) {
		rc = gpio_request(pdata->ndi_5v_en, "ndi_5v_en");
		if (!rc) {
			rc = gpio_direction_output(pdata->ndi_5v_en, 0);
			if (!rc) {
				gpio_set_value(pdata->ndi_5v_en, 1);
				gpioEnabled = true;
				msleep(20);
			}
		}
	}
	if (!gpioEnabled) {
		pr_err("%s NDI_5V_EN gpio failed to allocate\n", __func__);
		gpio_free(pdata->ndi_5v_en);
	}
	pdata->initDone = true;
	pr_debug("%s rc:%d\n", __func__, rc);
	return rc;
}

static void qxr_stdalonevwr_free_res(void)
{
	if (pdata->initDone) {
		if (pdata->reg_imu) {
			regulator_disable(pdata->reg_imu);
			devm_regulator_put(pdata->reg_imu);
		}
		gpio_free(pdata->ndi_5v_en);
		pdata->initDone = false;
	}
	pr_debug("%s initDone:%d\n", __func__, pdata->initDone);
}

static int qxr_stdalonevwr_probe(struct platform_device *pdev)
{
	pdata = devm_kzalloc(&pdev->dev, sizeof(struct qxr_stdalonevwr),
						GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->pdev = pdev;
	pdata->ndi_5v_en = 1237;
	pdata->initDone = false;
	qxr_stdalonevwr_allocate_res();
	pr_info("%s done\n", __func__);
	return 0;
}

static int qxr_stdalonevrw_pm_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	if (pdata)
		qxr_stdalonevwr_free_res();
	return 0;
}

static int qxr_stdalonevrw_pm_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	if (pdata)
		qxr_stdalonevwr_allocate_res();
	return 0;
}

static int qxr_stdalonevrw_pm_freeze(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return qxr_stdalonevrw_pm_suspend(dev);
};

static int qxr_stdalonevrw_pm_restore(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return qxr_stdalonevrw_pm_resume(dev);
};

static int qxr_stdalonevrw_pm_poweroff(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return qxr_stdalonevrw_pm_suspend(dev);
};

static int qxr_stdalonevrw_pm_thaw(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return qxr_stdalonevrw_pm_resume(dev);
};

static const struct dev_pm_ops qxr_stdalonevwr_dev_pm_ops = {
	.suspend = qxr_stdalonevrw_pm_suspend,
	.resume = qxr_stdalonevrw_pm_resume,
	.freeze = qxr_stdalonevrw_pm_freeze,
	.restore = qxr_stdalonevrw_pm_restore,
	.thaw = qxr_stdalonevrw_pm_thaw,
	.poweroff = qxr_stdalonevrw_pm_poweroff,
};

static const struct of_device_id qxr_stdalonevwr_match_table[] = {
	{ .compatible = "qcom,xr-stdalonevwr-misc", },
	{}
};

MODULE_DEVICE_TABLE(of, qxr_stdalonevwr_match_table);

static struct platform_driver qxr_stdalonevwr_driver = {
	.driver = {
		.name           = "qxr-stdalonevwr",
		.of_match_table = qxr_stdalonevwr_match_table,
		.pm = &qxr_stdalonevwr_dev_pm_ops,
	},
	.probe          = qxr_stdalonevwr_probe,
};

module_platform_driver(qxr_stdalonevwr_driver);

MODULE_DESCRIPTION("QTI XR STANDALONE MISC driver");
MODULE_LICENSE("GPL v2");
