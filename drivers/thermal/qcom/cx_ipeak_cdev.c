/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/io.h>

#define CXIP_LM_CDEV_DRIVER "cx-ipeak-cooling-device"
#define CXIP_LM_CDEV_MAX_STATE 1

#define CXIP_LM_VOTE_STATUS       0x0
#define CXIP_LM_BYPASS            0x4
#define CXIP_LM_VOTE_CLEAR        0x8
#define CXIP_LM_VOTE_SET          0xc
#define CXIP_LM_FEATURE_EN        0x10
#define CXIP_LM_BYPASS_VAL        0xff20
#define CXIP_LM_THERM_VOTE_VAL    0x80
#define CXIP_LM_FEATURE_EN_VAL    0x1

struct cxip_lm_cooling_device {
	struct thermal_cooling_device	*cool_dev;
	char				cdev_name[THERMAL_NAME_LENGTH];
	void				*cx_ip_reg_base;
	bool				state;
};

static void cxip_lm_therm_vote_apply(void *reg_base, bool vote)
{
	writel_relaxed(CXIP_LM_THERM_VOTE_VAL,
		reg_base +
		(vote ? CXIP_LM_VOTE_SET : CXIP_LM_VOTE_CLEAR));

	pr_debug("%s vote for cxip_lm. Agg.vote:0x%x\n",
		vote ? "Applied" : "Cleared",
		readl_relaxed(reg_base + CXIP_LM_VOTE_STATUS));
}

static void cxip_lm_initialize_cxip_hw(void *reg_base)
{
	/* Enable CXIP LM HW */
	writel_relaxed(CXIP_LM_FEATURE_EN_VAL, reg_base + CXIP_LM_FEATURE_EN);

	/* Set CXIP LM proxy vote for clients who are not participating */
	writel_relaxed(CXIP_LM_BYPASS_VAL, reg_base + CXIP_LM_BYPASS);
}

static int cxip_lm_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = CXIP_LM_CDEV_MAX_STATE;

	return 0;
}

static int cxip_lm_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct cxip_lm_cooling_device *cxip_dev = cdev->devdata;
	int ret = 0;

	if (state > CXIP_LM_CDEV_MAX_STATE)
		state = CXIP_LM_CDEV_MAX_STATE;

	if (cxip_dev->state == state)
		return 0;

	cxip_lm_therm_vote_apply(cxip_dev->cx_ip_reg_base, state);
	cxip_dev->state = state;

	return ret;
}

static int cxip_lm_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct cxip_lm_cooling_device *cxip_dev = cdev->devdata;

	*state = cxip_dev->state;

	return 0;
}

static struct thermal_cooling_device_ops cxip_lm_device_ops = {
	.get_max_state = cxip_lm_get_max_state,
	.get_cur_state = cxip_lm_get_cur_state,
	.set_cur_state = cxip_lm_set_cur_state,
};

static int cxip_lm_cdev_remove(struct platform_device *pdev)
{
	struct cxip_lm_cooling_device *cxip_dev =
		(struct cxip_lm_cooling_device *)dev_get_drvdata(&pdev->dev);

	if (cxip_dev) {
		if (cxip_dev->cool_dev)
			thermal_cooling_device_unregister(cxip_dev->cool_dev);

		if (cxip_dev->cx_ip_reg_base)
			cxip_lm_therm_vote_apply(cxip_dev->cx_ip_reg_base,
							false);
	}

	return 0;
}

static int cxip_lm_cdev_probe(struct platform_device *pdev)
{
	struct cxip_lm_cooling_device *cxip_dev = NULL;
	int ret = 0;
	struct device_node *np;
	struct resource *res = NULL;

	np = dev_of_node(&pdev->dev);
	if (!np) {
		dev_err(&pdev->dev,
			"of node not available for cxip_lm cdev\n");
		return -EINVAL;
	}

	cxip_dev = devm_kzalloc(&pdev->dev, sizeof(*cxip_dev), GFP_KERNEL);
	if (!cxip_dev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev,
			"cxip_lm platform get resource failed\n");
		return -ENODEV;
	}

	cxip_dev->cx_ip_reg_base = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
	if (!cxip_dev->cx_ip_reg_base) {
		dev_err(&pdev->dev, "cxip_lm reg remap failed\n");
		return -ENOMEM;
	}

	cxip_lm_initialize_cxip_hw(cxip_dev->cx_ip_reg_base);

	/* Set thermal vote till we get first vote from TF */
	cxip_dev->state = true;
	cxip_lm_therm_vote_apply(cxip_dev->cx_ip_reg_base,
					cxip_dev->state);

	strlcpy(cxip_dev->cdev_name, np->name, THERMAL_NAME_LENGTH);
	cxip_dev->cool_dev = thermal_of_cooling_device_register(
					np, cxip_dev->cdev_name, cxip_dev,
					&cxip_lm_device_ops);
	if (IS_ERR(cxip_dev->cool_dev)) {
		ret = PTR_ERR(cxip_dev->cool_dev);
		dev_err(&pdev->dev, "cxip_lm cdev register err:%d\n",
				ret);
		cxip_dev->cool_dev = NULL;
		cxip_lm_therm_vote_apply(cxip_dev->cx_ip_reg_base,
						false);
		return ret;
	}

	dev_set_drvdata(&pdev->dev, cxip_dev);

	return ret;
}

static const struct of_device_id cxip_lm_cdev_of_match[] = {
	{.compatible = "qcom,cxip-lm-cooling-device", },
	{}
};

static struct platform_driver cxip_lm_cdev_driver = {
	.driver = {
		.name = CXIP_LM_CDEV_DRIVER,
		.of_match_table = cxip_lm_cdev_of_match,
	},
	.probe = cxip_lm_cdev_probe,
	.remove = cxip_lm_cdev_remove,
};
builtin_platform_driver(cxip_lm_cdev_driver);
