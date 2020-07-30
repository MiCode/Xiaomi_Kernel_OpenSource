// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include "md_cooling.h"

#define DEFAULT_THROTTLE_TX_PWR_LV1	(4)
#define DEFAULT_THROTTLE_TX_PWR_LV2	(6)
#define DEFAULT_THROTTLE_TX_PWR_LV3	(8)

static int md_cooling_tx_pwr_get_max_state(
	struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->max_level;

	return 0;
}

static int md_cooling_tx_pwr_get_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->target_level;

	return 0;
}

static int md_cooling_tx_pwr_set_cur_state(
		struct thermal_cooling_device *cdev, unsigned long state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;
	struct device *dev = (struct device *)md_cdev->dev_data;
	enum md_status status;
	unsigned int msg, pwr;
	int ret = 0;

	/* Request state should be less than max_level */
	if (WARN_ON(state > md_cdev->max_level))
		return -EINVAL;

	if (md_cdev->target_level == state)
		return 0;

	status = get_md_status();
	if (is_md_inactive(status)) {
		dev_info(dev, "skip tx pwr control due to MD is inactive\n");
		if (is_md_off(status))
			md_cdev->target_level = MD_COOLING_UNLIMITED_LV;
		return -EACCES;
	}

	pwr = (state == MD_COOLING_UNLIMITED_LV)
		? 0 : md_cdev->throttle_tx_power[state - 1];
	msg = reduce_tx_pwr_to_tmc_msg(md_cdev->pa_id, pwr);
	ret = send_throttle_msg(msg);
	if (!ret)
		md_cdev->target_level = state;

	dev_dbg(dev, "%s: set lv = %ld done\n", md_cdev->name, state);

	return ret;
}

static const struct of_device_id md_cooling_tx_pwr_of_match[] = {
	{ .compatible = "mediatek,md-cooler-tx-pwr", },
	{},
};
MODULE_DEVICE_TABLE(of, md_cooling_tx_pwr_of_match);

static struct thermal_cooling_device_ops md_cooling_tx_pwr_ops = {
	.get_max_state		= md_cooling_tx_pwr_get_max_state,
	.get_cur_state		= md_cooling_tx_pwr_get_cur_state,
	.set_cur_state		= md_cooling_tx_pwr_set_cur_state,
};

static int md_cooling_tx_pwr_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	unsigned int throttle_tx_pwr[MAX_NUM_TX_PWR_LV] = {
		DEFAULT_THROTTLE_TX_PWR_LV1,
		DEFAULT_THROTTLE_TX_PWR_LV2,
		DEFAULT_THROTTLE_TX_PWR_LV3,
	};
	int ret = -1;

	if (!np) {
		dev_err(dev, "MD cooler DT node not found\n");
		return -ENODEV;
	}

	ret = md_cooling_register(np,
				MD_COOLING_TYPE_TX_PWR,
				MAX_NUM_TX_PWR_LV,
				throttle_tx_pwr,
				&md_cooling_tx_pwr_ops,
				dev);
	if (ret) {
		dev_err(dev, "register tx-pwr cdev failed!\n");
		return ret;
	}

	return ret;
}

static int md_cooling_tx_pwr_remove(struct platform_device *pdev)
{
	md_cooling_unregister(MD_COOLING_TYPE_TX_PWR);

	return 0;
}

static struct platform_driver md_cooling_tx_pwr_driver = {
	.probe = md_cooling_tx_pwr_probe,
	.remove = md_cooling_tx_pwr_remove,
	.driver = {
		.name = "mtk-md-cooling-tx-pwr",
		.of_match_table = md_cooling_tx_pwr_of_match,
	},
};
module_platform_driver(md_cooling_tx_pwr_driver);

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek modem cooling TX power throttle driver");
MODULE_LICENSE("GPL v2");
