/*
 * drivers/video/tegra/host/gr2d/gr2d.c
 *
 * Tegra Graphics 2D
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/export.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include "dev.h"
#include "bus_client.h"
#include "gr2d_t30.h"
#include "gr2d_t114.h"
#include "t20/t20.h"
#include "t30/t30.h"
#include "t114/t114.h"

static struct of_device_id tegra_gr2d_of_match[] __devinitdata = {
	{ .compatible = "nvidia,tegra20-gr2d",
		.data = (struct nvhost_device_data *)&t20_gr2d_info },
	{ .compatible = "nvidia,tegra30-gr2d",
		.data = (struct nvhost_device_data *)&t30_gr2d_info },
	{ .compatible = "nvidia,tegra114-gr2d",
		.data = (struct nvhost_device_data *)&t11_gr2d_info },
	{ },
};
static int __devinit gr2d_probe(struct platform_device *dev)
{
	int err = 0;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_gr2d_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}
	pdata->pdev = dev;
	platform_set_drvdata(dev, pdata);

	err = nvhost_client_device_init(dev);
	if (err)
		return err;

	pm_runtime_use_autosuspend(&dev->dev);
	pm_runtime_set_autosuspend_delay(&dev->dev, 100);
	pm_runtime_enable(&dev->dev);

	return 0;
}

static int __exit gr2d_remove(struct platform_device *dev)
{
	/* Add clean-up */
	return 0;
}

#ifdef CONFIG_PM
static int gr2d_suspend(struct platform_device *dev, pm_message_t state)
{
	return nvhost_client_device_suspend(dev);
}

static int gr2d_resume(struct platform_device *dev)
{
	dev_info(&dev->dev, "resuming\n");
	return 0;
}
#endif

static struct platform_driver gr2d_driver = {
	.probe = gr2d_probe,
	.remove = __exit_p(gr2d_remove),
#ifdef CONFIG_PM
	.suspend = gr2d_suspend,
	.resume = gr2d_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "gr2d",
#ifdef CONFIG_OF
		.of_match_table = tegra_gr2d_of_match,
#endif
	},
};

static int __init gr2d_init(void)
{
	return platform_driver_register(&gr2d_driver);
}

static void __exit gr2d_exit(void)
{
	platform_driver_unregister(&gr2d_driver);
}

module_init(gr2d_init);
module_exit(gr2d_exit);
