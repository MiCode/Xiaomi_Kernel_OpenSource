// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Xiaomi, Inc.
 */

#include <linux/delay.h>
#include "zisp_rproc_utils.h"

static int zisp_rproc_init(struct platform_device *pdev)
{
	struct zisp_rproc *zisp_rproc;
	struct zisp_ops *zisp_ops;

	zisp_rproc = platform_get_drvdata(pdev);
	zisp_ops = zisp_rproc->zisp_ops;

	if (zisp_ops && zisp_ops->init)
		return zisp_ops->init(zisp_rproc);

	return 0;
}

static void zisp_rproc_shutdown(struct platform_device *pdev)
{
	struct zisp_rproc *zisp_rproc;
	struct zisp_ops *zisp_ops;

	zisp_rproc = platform_get_drvdata(pdev);
	zisp_ops = zisp_rproc->zisp_ops;

	if (zisp_ops && zisp_ops->shutdown)
		zisp_ops->shutdown(zisp_rproc);
}

static int zisp_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc;
	struct zisp_rproc *zisp_rproc;
	struct zisp_ops *zisp_ops;

	zisp_rproc = platform_get_drvdata(pdev);
	rproc = zisp_rproc->rproc;
	zisp_rproc_shutdown(pdev);
	zisp_ops = zisp_rproc->zisp_ops;
	rproc_del(rproc);
	rproc_free(rproc);
	if (zisp_ops && zisp_ops->remove)
		zisp_ops->remove(zisp_rproc);

	return 0;
}

static const struct platform_device_id zisp_mcu_rproc_match[] = {
	{ .name = "ispv3-rpmsg_pci", (unsigned long)&zisp3_ops_pci, },
	{ .name = "ispv3-rpmsg_spi", (unsigned long)&zisp3_ops_spi, },
	{ /* sentinel */ }
};

static int zisp_rproc_probe(struct platform_device *pdev)
{
	struct zisp_rproc *zisp_rproc;
	struct zisp_ops *zisp_ops;
	struct rproc *rproc;
	int ret;

	zisp_ops = (struct zisp_ops *)
		    platform_get_device_id(pdev)->driver_data;

	if (!zisp_ops || !zisp_ops->rproc_ops)
		return -ENODEV;
	if (!zisp_ops->firmware)
		return -ENODEV;

	rproc = rproc_alloc(&pdev->dev, dev_name(&pdev->dev),
			    zisp_ops->rproc_ops, zisp_ops->firmware,
			    sizeof(*zisp_rproc));
	if (!rproc) {
		dev_err(&pdev->dev, "rproc_alloc failed\n");
		ret = -ENOMEM;
		goto alloc_failed;
	}
#ifndef CONFIG_ZISP_OCRAM_AON
	rproc->auto_boot = false;
#else
	rproc->auto_boot = true;
#endif
	zisp_rproc = rproc->priv;
	zisp_rproc->zisp_ops = zisp_ops;
	zisp_rproc->dev = &pdev->dev;
	zisp_rproc->rproc = rproc;
	platform_set_drvdata(pdev, zisp_rproc);

	ret = zisp_rproc_init(pdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "rproc init failed\n");
		goto free_rproc;
	}

	/* add assert reset and power on but not start here */

	ret = rproc_add(rproc);
	if (ret < 0) {
		dev_err(&pdev->dev, "rproc_add failed: %d\n", ret);
		goto power_off;
	}

	return 0;

power_off:
	/* add power off here */
free_rproc:
	rproc_free(rproc);
alloc_failed:
	return ret;
}

static struct platform_driver zisp_rproc_driver = {
	.driver = {
		.name = "xiaomi-zisp-rproc",
		.owner = THIS_MODULE,
	},
	.probe = zisp_rproc_probe,
	.remove = zisp_rproc_remove,
	.shutdown = zisp_rproc_shutdown,
	.id_table = zisp_mcu_rproc_match,
};

module_platform_driver(zisp_rproc_driver);

MODULE_AUTHOR("ZhongAn<zhongan@xiaomi.com>");
MODULE_DESCRIPTION("Xiaomi ZISP MCU Remote Processor Driver");
MODULE_LICENSE("GPL v2");
