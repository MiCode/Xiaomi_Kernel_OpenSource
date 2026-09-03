/*
 * Copyright (C) 2020 SPRD Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/hw_random.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/random.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd_trng: " fmt

#define RNG_DATABUF_SIZE 32
#define CE_TRNG_DATA_REG_SIZE 4

/**reg**/
#define REG_CE_CLK              0x018
#define REG_CE_VERSION          0x0c8
#define REG_CE_RNG_EN           0x200
#define REG_CE_RNG_CONFIG       0x204
#define REG_CE_RNG_DATA         0x208
#define REG_CE_RNG_WORK_STATUS  0x214
#define REG_CE_RNG_POST         0x210
#define REG_CE_RNG_MODE         0x228
#define REG_CE_RNG_RING_L       0x248
#define REG_CE_RNG_RING_H       0x24c

/**cfg**/
#define TRNG_CLK_EN         (0x1 << 24)
#define TRNG_CLK_DIS        (0x0 << 24)
#define CE_CFG              (0x1 << 24)
#define RNG_ENABLE          0x1FF03
#define RNG_DISABLE         0x0
//#define RNG_CFG             0xFFF00363
#define RNG_DATA_VALID      0x2
#define TRNG_MODE           0x1

#define to_sprd_trng(p)	container_of(p, struct sprd_trng, rng)

struct sprd_trng {
	void __iomem *base;
	struct hwrng rng;
	struct clk *clk;
	struct clk *ce_eb;
	struct clk *parent;
	struct device *dev;
};

static int sprd_trng_init(struct hwrng *rng)
{
	struct sprd_trng *trng = to_sprd_trng(rng);
	u32 val = 0;
	u32 err = 0;
	err = clk_prepare_enable(trng->clk);
	if (err)
		return err;
	err = clk_prepare_enable(trng->ce_eb);
	if (err)
		return err;

	/*enable trng clk*/
	val = readl_relaxed(trng->base + REG_CE_CLK);
	val |= TRNG_CLK_EN;
	writel_relaxed(val, trng->base + REG_CE_CLK);


	/*enable rng*/
	val = readl_relaxed(trng->base + REG_CE_RNG_EN);
	val |= RNG_ENABLE;
	writel_relaxed(val, trng->base + REG_CE_RNG_EN);

	val = readl_relaxed(trng->base + REG_CE_RNG_MODE);
	val |= TRNG_MODE;
	writel_relaxed(val, trng->base + REG_CE_RNG_MODE);

	return 0;
}

static void sprd_trng_cleanup(struct hwrng *rng)
{
	struct sprd_trng *trng = to_sprd_trng(rng);
	u32 val = 0;

	/*disable rng*/
	val = readl_relaxed(trng->base + REG_CE_RNG_EN);
	val &= RNG_DISABLE;
	writel_relaxed(val, trng->base + REG_CE_RNG_EN);

	/*disable trng clk*/
	val = readl_relaxed(trng->base + REG_CE_CLK);
	val &= TRNG_CLK_DIS;
	writel_relaxed(val, trng->base + REG_CE_CLK);
	clk_disable_unprepare(trng->ce_eb);
	clk_disable_unprepare(trng->clk);
}

static int sprd_trng_read(struct hwrng *rng, void *buf, size_t max, bool wait)
{
	struct sprd_trng *trng = to_sprd_trng(rng);
	u32 *data = (u32 *)buf;
	int index = 0, rng_not_got = 0, count;

	if (max > RNG_DATABUF_SIZE)
		max = RNG_DATABUF_SIZE;
	if (max < 1)
		return 0;

	pm_runtime_get_sync(trng->dev);
	do {
		if (readl_relaxed(trng->base + REG_CE_RNG_WORK_STATUS) & RNG_DATA_VALID) {
			count = (max - 1) / CE_TRNG_DATA_REG_SIZE + 1;
			while (count--)
				data[index++] = readl_relaxed(trng->base + REG_CE_RNG_DATA);
			rng_not_got = 0;
		} else {
			pr_info("RNG doesn't generate random!\n");
			rng_not_got++;
			msleep(1);
		}
	} while (rng_not_got > 0 && rng_not_got < 10);
	pm_runtime_put(trng->dev);

	return index * CE_TRNG_DATA_REG_SIZE;
}

static int sprd_trng_probe(struct platform_device *pdev)
{
	struct sprd_trng *rng;
	struct resource *res;
	int ret = 0;

	rng = devm_kzalloc(&pdev->dev, sizeof(*rng), GFP_KERNEL);
	if (!rng)
		return -ENOMEM;

	platform_set_drvdata(pdev, rng);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rng->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rng->base))
		return PTR_ERR(rng->base);

	/*set ce clk*/
	rng->clk = devm_clk_get(&pdev->dev, "ap-ce-clk");
	if (IS_ERR(rng->clk))
		return PTR_ERR(rng->clk);

	rng->ce_eb = devm_clk_get(&pdev->dev, "ce-pub-eb");
	if (IS_ERR(rng->ce_eb))
		return PTR_ERR(rng->ce_eb);

	rng->parent = devm_clk_get(&pdev->dev, "source");
	if (IS_ERR(rng->parent))
		return PTR_ERR(rng->parent);

	clk_set_parent(rng->clk, rng->parent);

	rng->dev = &pdev->dev;

	rng->rng.name = pdev->name;
	rng->rng.init = sprd_trng_init;
	rng->rng.cleanup = sprd_trng_cleanup;
	rng->rng.read = sprd_trng_read;

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = devm_hwrng_register(&pdev->dev, &rng->rng);
	if (ret) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		return ret;
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int sprd_suspend(struct device *dev)
{
	struct sprd_trng *trng = dev_get_drvdata(dev);

	if (pm_runtime_status_suspended(dev))
		return 0;

	sprd_trng_cleanup(&(trng->rng));

	return 0;
}

static int sprd_resume(struct device *dev)
{
	u32 err;
	struct sprd_trng *trng = dev_get_drvdata(dev);

	if (pm_runtime_status_suspended(dev))
		return 0;

	err = sprd_trng_init(&(trng->rng));
	if (err) {
		clk_disable_unprepare(trng->clk);
		dev_err(dev, "pubce resume failed!\n");
		return err;
	}

	return 0;
}
#endif

static UNIVERSAL_DEV_PM_OPS(sprd_pm_ops, sprd_suspend, sprd_resume, NULL);

static const struct of_device_id sprd_trng_dt_ids[] = {
	{ .compatible = "sprd,sprd-trng" },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_trng_dt_ids);

static struct platform_driver sprd_trng_driver = {
	.probe		= sprd_trng_probe,
	.driver		= {
		.name	= "sprd_trng",
		.pm = &sprd_pm_ops,
		.of_match_table = of_match_ptr(sprd_trng_dt_ids),
	},
};

module_platform_driver(sprd_trng_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Guozhu Xing <guozhu.xing@unisoc.com>");
MODULE_DESCRIPTION("Unisoc true random number generator driver");
