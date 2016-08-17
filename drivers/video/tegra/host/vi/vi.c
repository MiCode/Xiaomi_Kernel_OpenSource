/*
 * drivers/video/tegra/host/vi/vi.c
 *
 * Tegra Graphics Host VI
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION, All rights reserved.
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
#include <linux/resource.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>

#include <mach/iomap.h>

#include "dev.h"
#include "bus_client.h"
#include "t20/t20.h"
#include "t30/t30.h"
#include "t114/t114.h"
#include "vi.h"

static struct of_device_id tegra_vi_of_match[] __devinitdata = {
	{ .compatible = "nvidia,tegra20-vi",
		.data = (struct nvhost_device_data *)&t20_vi_info },
	{ .compatible = "nvidia,tegra30-vi",
		.data = (struct nvhost_device_data *)&t30_vi_info },
	{ .compatible = "nvidia,tegra114-vi",
		.data = (struct nvhost_device_data *)&t11_vi_info },
	{ },
};
static int __devinit vi_probe(struct platform_device *dev)
{
	int err = 0;
	struct vi *tegra_vi;
	struct nvhost_device_data *pdata = NULL;

	if (dev->dev.of_node) {
		const struct of_device_id *match;

		match = of_match_device(tegra_vi_of_match, &dev->dev);
		if (match)
			pdata = (struct nvhost_device_data *)match->data;
	} else
		pdata = (struct nvhost_device_data *)dev->dev.platform_data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(&dev->dev, "no platform data\n");
		return -ENODATA;
	}

	dev_info(&dev->dev, "%s: ++\n", __func__);
	tegra_vi = kzalloc(sizeof(struct vi), GFP_KERNEL);
	if (!tegra_vi) {
		dev_err(&dev->dev, "can't allocate memory for vi\n");
		return -ENOMEM;
	}

	tegra_vi->ndev = dev;
	pdata->private_data = tegra_vi;

#ifdef CONFIG_TEGRA_CAMERA
	tegra_vi->camera = tegra_camera_register(dev);
	if (!tegra_vi->camera) {
		dev_err(&dev->dev, "%s: can't register tegra_camera\n",
				__func__);
		goto camera_register_fail;
	}
#endif
	pdata->pdev = dev;
	platform_set_drvdata(dev, pdata);
	err = nvhost_client_device_get_resources(dev);
	if (err) {
		goto camera_register_fail;
	}

	err = nvhost_client_device_init(dev);
	if (err)
		goto camera_register_fail;

	pm_runtime_use_autosuspend(&dev->dev);
	pm_runtime_set_autosuspend_delay(&dev->dev, 100);
	pm_runtime_enable(&dev->dev);

	return 0;

camera_register_fail:
	kfree(tegra_vi);
	return err;
}

static int __exit vi_remove(struct platform_device *dev)
{
#ifdef CONFIG_TEGRA_CAMERA
	int err = 0;
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(dev);
	struct vi *tegra_vi = (struct vi *)pdata->private_data;
#endif

	dev_info(&dev->dev, "%s: ++\n", __func__);

#ifdef CONFIG_TEGRA_CAMERA
	err = tegra_camera_unregister(tegra_vi->camera);
	if (err)
		return err;
#endif

	return 0;
}

#ifdef CONFIG_PM
static int vi_suspend(struct platform_device *dev, pm_message_t state)
{
#ifdef CONFIG_TEGRA_CAMERA
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(dev);
	struct vi *tegra_vi = (struct vi *)pdata->private_data;
#endif

	dev_info(&dev->dev, "%s: ++\n", __func__);

#ifdef CONFIG_TEGRA_CAMERA
	tegra_camera_suspend(tegra_vi->camera);
#endif

	return nvhost_client_device_suspend(dev);
}

static int vi_resume(struct platform_device *dev)
{
#ifdef CONFIG_TEGRA_CAMERA
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(dev);

	struct vi *tegra_vi = (struct vi *)pdata->private_data;
#endif

	dev_info(&dev->dev, "%s: ++\n", __func__);

#ifdef CONFIG_TEGRA_CAMERA
	tegra_camera_resume(tegra_vi->camera);
#endif

	return 0;
}
#endif

static struct platform_driver vi_driver = {
	.probe = vi_probe,
	.remove = __exit_p(vi_remove),
#ifdef CONFIG_PM
	.suspend = vi_suspend,
	.resume = vi_resume,
#endif
	.driver = {
		.owner = THIS_MODULE,
		.name = "vi",
#ifdef CONFIG_OF
		.of_match_table = tegra_vi_of_match,
#endif
	}
};

static int __init vi_init(void)
{
	return platform_driver_register(&vi_driver);
}

static void __exit vi_exit(void)
{
	platform_driver_unregister(&vi_driver);
}

late_initcall(vi_init);
module_exit(vi_exit);
