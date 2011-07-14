/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/hw_random.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/types.h>

#define DRIVER_NAME "msm_rng"

/* Device specific register offsets */
#define PRNG_DATA_OUT_OFFSET    0x0000
#define PRNG_STATUS_OFFSET	0x0004
#define PRNG_LFSR_CFG_OFFSET	0x0100
#define PRNG_CONFIG_OFFSET	0x0104

/* Device specific register masks and config values */
#define PRNG_LFSR_CFG_MASK	0xFFFF0000
#define PRNG_LFSR_CFG_CLOCKS	0x0000DDDD
#define PRNG_CONFIG_MASK	0xFFFFFFFD
#define PRNG_CONFIG_ENABLE	0x00000002

#define MAX_HW_FIFO_DEPTH 16                     /* FIFO is 16 words deep */
#define MAX_HW_FIFO_SIZE (MAX_HW_FIFO_DEPTH * 4) /* FIFO is 32 bits wide  */

struct msm_rng_device {
	struct platform_device *pdev;
	void __iomem *base;
	struct clk *prng_clk;
};

static int msm_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	struct msm_rng_device *msm_rng_dev;
	struct platform_device *pdev;
	void __iomem *base;
	size_t maxsize;
	size_t currsize = 0;
	unsigned long val;
	unsigned long *retdata = data;
	int ret;

	msm_rng_dev = (struct msm_rng_device *)rng->priv;
	pdev = msm_rng_dev->pdev;
	base = msm_rng_dev->base;

	/* calculate max size bytes to transfer back to caller */
	maxsize = min_t(size_t, MAX_HW_FIFO_SIZE, max);

	/* no room for word data */
	if (maxsize < 4)
		return 0;

	/* enable PRNG clock */
	ret = clk_enable(msm_rng_dev->prng_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clock in callback\n");
		return 0;
	}

	/* read random data from h/w */
	do {
		/* check status bit if data is available */
		if (!(readl(base + PRNG_STATUS_OFFSET) & 0x00000001))
			break;	/* no data to read so just bail */

		/* read FIFO */
		val = readl(base + PRNG_DATA_OUT_OFFSET);
		if (!val)
			break;	/* no data to read so just bail */

		/* write data back to callers pointer */
		*(retdata++) = val;
		currsize += 4;

		/* make sure we stay on 32bit boundary */
		if ((maxsize - currsize) < 4)
			break;
	} while (currsize < maxsize);

	/* vote to turn off clock */
	clk_disable(msm_rng_dev->prng_clk);

	return currsize;
}

static struct hwrng msm_rng = {
	.name = DRIVER_NAME,
	.read = msm_rng_read,
};

static int __devinit msm_rng_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct msm_rng_device *msm_rng_dev = NULL;
	void __iomem *base = NULL;
	int error = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "invalid address\n");
		error = -EFAULT;
		goto err_exit;
	}

	msm_rng_dev = kzalloc(sizeof(msm_rng_dev), GFP_KERNEL);
	if (!msm_rng_dev) {
		dev_err(&pdev->dev, "cannot allocate memory\n");
		error = -ENOMEM;
		goto err_exit;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&pdev->dev, "ioremap failed\n");
		error = -ENOMEM;
		goto err_iomap;
	}
	msm_rng_dev->base = base;

	/* create a handle for clock control */
	msm_rng_dev->prng_clk = clk_get(NULL, "prng_clk");
	if (IS_ERR(msm_rng_dev->prng_clk)) {
		dev_err(&pdev->dev, "failed to register clock source\n");
		error = -EPERM;
		goto err_clk_get;
	}

	/* save away pdev and register driver data */
	msm_rng_dev->pdev = pdev;
	platform_set_drvdata(pdev, msm_rng_dev);

	/* register with hwrng framework */
	msm_rng.priv = (unsigned long) msm_rng_dev;
	error = hwrng_register(&msm_rng);
	if (error) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		error = -EPERM;
		goto err_hw_register;
	}

	return 0;

err_hw_register:
	clk_put(msm_rng_dev->prng_clk);
err_clk_get:
	iounmap(msm_rng_dev->base);
err_iomap:
	kfree(msm_rng_dev);
err_exit:
	return error;
}

static int __devexit msm_rng_remove(struct platform_device *pdev)
{
	struct msm_rng_device *msm_rng_dev = platform_get_drvdata(pdev);

	hwrng_unregister(&msm_rng);
	clk_put(msm_rng_dev->prng_clk);
	iounmap(msm_rng_dev->base);
	platform_set_drvdata(pdev, NULL);
	kfree(msm_rng_dev);
	return 0;
}

static struct platform_driver rng_driver = {
	.probe      = msm_rng_probe,
	.remove     = __devexit_p(msm_rng_remove),
	.driver     = {
		.name   = DRIVER_NAME,
		.owner  = THIS_MODULE,
	}
};

static int __init msm_rng_init(void)
{
	return platform_driver_register(&rng_driver);
}

module_init(msm_rng_init);

static void __exit msm_rng_exit(void)
{
	platform_driver_unregister(&rng_driver);
}

module_exit(msm_rng_exit);

MODULE_AUTHOR("Code Aurora Forum");
MODULE_DESCRIPTION("Qualcomm MSM Random Number Driver");
MODULE_LICENSE("GPL v2");
