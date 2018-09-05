/*
 * Copyright (c) 2011-2013, 2015, 2017 The Linux Foundation. All rights
 * reserved.
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
#include <soc/qcom/socinfo.h>
#include <linux/msm-bus.h>
#include <linux/qrng.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <crypto/internal/rng.h>

#include <linux/platform_data/qcom_crypto_device.h>



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
#define PRNG_HW_ENABLE		0x00000002

#define MAX_HW_FIFO_DEPTH 16                     /* FIFO is 16 words deep */
#define MAX_HW_FIFO_SIZE (MAX_HW_FIFO_DEPTH * 4) /* FIFO is 32 bits wide  */

#define RETRY_MAX_CNT		20	/* max retry times to read register */
#define RETRY_DELAY_INTERVAL	440	/* retry delay interval in us */

struct msm_rng_device {
	struct platform_device *pdev;
	void __iomem *base;
	struct clk *prng_clk;
	uint32_t qrng_perf_client;
	struct mutex rng_lock;
};

struct msm_rng_device msm_rng_device_info;
static struct msm_rng_device *msm_rng_dev_cached;
struct mutex cached_rng_lock;
static long msm_rng_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case QRNG_IOCTL_RESET_BUS_BANDWIDTH:
		pr_info("calling msm_rng_bus_scale(LOW)\n");
		ret = msm_bus_scale_client_update_request(
				msm_rng_device_info.qrng_perf_client, 0);
		if (ret)
			pr_err("failed qrng_reset_bus_bw, ret = %ld\n", ret);
		break;
	default:
		pr_err("Unsupported IOCTL call");
		break;
	}
	return ret;
}

/*
 *
 *  This function calls hardware random bit generator directory and retuns it
 *  back to caller
 *
 */
static int msm_rng_direct_read(struct msm_rng_device *msm_rng_dev,
					void *data, size_t max)
{
	struct platform_device *pdev;
	void __iomem *base;
	size_t currsize = 0;
	u32 val = 0;
	u32 *retdata = data;
	int ret;
	int failed = 0;

	pdev = msm_rng_dev->pdev;
	base = msm_rng_dev->base;

	/* no room for word data */
	if (max < 4)
		return 0;

	mutex_lock(&msm_rng_dev->rng_lock);

	if (msm_rng_dev->qrng_perf_client) {
		ret = msm_bus_scale_client_update_request(
				msm_rng_dev->qrng_perf_client, 1);
		if (ret) {
			pr_err("bus_scale_client_update_req failed!\n");
			goto bus_err;
		}
	}
	/* enable PRNG clock */
	ret = clk_prepare_enable(msm_rng_dev->prng_clk);
	if (ret) {
		pr_err("failed to enable prng clock\n");
		goto err;
	}
	/* read random data from h/w */
	do {
		/* check status bit if data is available */
		if (!(readl_relaxed(base + PRNG_STATUS_OFFSET)
				& 0x00000001)) {
			if (failed++ == RETRY_MAX_CNT) {
				if (currsize == 0)
					pr_err("Data not available\n");
				break;
			}
			udelay(RETRY_DELAY_INTERVAL);
		} else {

			/* read FIFO */
			val = readl_relaxed(base + PRNG_DATA_OUT_OFFSET);

			/* write data back to callers pointer */
			*(retdata++) = val;
			currsize += 4;
			/* make sure we stay on 32bit boundary */
			if ((max - currsize) < 4)
				break;
		}

	} while (currsize < max);

	/* vote to turn off clock */
	clk_disable_unprepare(msm_rng_dev->prng_clk);
err:
	if (msm_rng_dev->qrng_perf_client) {
		ret = msm_bus_scale_client_update_request(
				msm_rng_dev->qrng_perf_client, 0);
		if (ret)
			pr_err("bus_scale_client_update_req failed!\n");
	}
bus_err:
	mutex_unlock(&msm_rng_dev->rng_lock);

	val = 0L;
	return currsize;
}
static int msm_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	struct msm_rng_device *msm_rng_dev;
	int rv = 0;

	msm_rng_dev = (struct msm_rng_device *)rng->priv;
	rv = msm_rng_direct_read(msm_rng_dev, data, max);

	return rv;
}


static struct hwrng msm_rng = {
	.name = DRIVER_NAME,
	.read = msm_rng_read,
	.quality = 700,
};

static int msm_rng_enable_hw(struct msm_rng_device *msm_rng_dev)
{
	unsigned long val = 0;
	unsigned long reg_val = 0;
	int ret = 0;

	if (msm_rng_dev->qrng_perf_client) {
		ret = msm_bus_scale_client_update_request(
				msm_rng_dev->qrng_perf_client, 1);
		if (ret)
			pr_err("bus_scale_client_update_req failed!\n");
	}
	/* Enable the PRNG CLK */
	ret = clk_prepare_enable(msm_rng_dev->prng_clk);
	if (ret) {
		dev_err(&(msm_rng_dev->pdev)->dev,
				"failed to enable clock in probe\n");
		return -EPERM;
	}

	/* Enable PRNG h/w only if it is NOT ON */
	val = readl_relaxed(msm_rng_dev->base + PRNG_CONFIG_OFFSET) &
					PRNG_HW_ENABLE;
	/* PRNG H/W is not ON */
	if (val != PRNG_HW_ENABLE) {
		val = readl_relaxed(msm_rng_dev->base + PRNG_LFSR_CFG_OFFSET);
		val &= PRNG_LFSR_CFG_MASK;
		val |= PRNG_LFSR_CFG_CLOCKS;
		writel_relaxed(val, msm_rng_dev->base + PRNG_LFSR_CFG_OFFSET);

		/* The PRNG CONFIG register should be first written */
		mb();

		reg_val = readl_relaxed(msm_rng_dev->base + PRNG_CONFIG_OFFSET)
						& PRNG_CONFIG_MASK;
		reg_val |= PRNG_HW_ENABLE;
		writel_relaxed(reg_val, msm_rng_dev->base + PRNG_CONFIG_OFFSET);

		/* The PRNG clk should be disabled only after we enable the
		 * PRNG h/w by writing to the PRNG CONFIG register.
		 */
		mb();
	}
	clk_disable_unprepare(msm_rng_dev->prng_clk);

	if (msm_rng_dev->qrng_perf_client) {
		ret = msm_bus_scale_client_update_request(
				msm_rng_dev->qrng_perf_client, 0);
		if (ret)
			pr_err("bus_scale_client_update_req failed!\n");
	}

	return 0;
}

static const struct file_operations msm_rng_fops = {
	.unlocked_ioctl = msm_rng_ioctl,
};
static struct class *msm_rng_class;
static struct cdev msm_rng_cdev;

static int msm_rng_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct msm_rng_device *msm_rng_dev = NULL;
	void __iomem *base = NULL;
	bool configure_qrng = true;
	int error = 0;
	int ret = 0;
	struct device *dev;

	struct msm_bus_scale_pdata *qrng_platform_support = NULL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "invalid address\n");
		error = -EFAULT;
		goto err_exit;
	}

	msm_rng_dev = kzalloc(sizeof(struct msm_rng_device), GFP_KERNEL);
	if (!msm_rng_dev) {
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
	if ((pdev->dev.of_node) && (of_property_read_bool(pdev->dev.of_node,
					"qcom,msm-rng-iface-clk")))
		msm_rng_dev->prng_clk = clk_get(&pdev->dev,
							"iface_clk");
	else
		msm_rng_dev->prng_clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(msm_rng_dev->prng_clk)) {
		dev_err(&pdev->dev, "failed to register clock source\n");
		error = -EPERM;
		goto err_clk_get;
	}

	/* save away pdev and register driver data */
	msm_rng_dev->pdev = pdev;
	platform_set_drvdata(pdev, msm_rng_dev);

	if (pdev->dev.of_node) {
		/* Register bus client */
		qrng_platform_support = msm_bus_cl_get_pdata(pdev);
		msm_rng_dev->qrng_perf_client = msm_bus_scale_register_client(
						qrng_platform_support);
		msm_rng_device_info.qrng_perf_client =
					msm_rng_dev->qrng_perf_client;
		if (!msm_rng_dev->qrng_perf_client)
			pr_err("Unable to register bus client\n");
	}

	/* Enable rng h/w for the targets which can access the entire
	 * address space of PRNG.
	 */
	if ((pdev->dev.of_node) && (of_property_read_bool(pdev->dev.of_node,
					"qcom,no-qrng-config")))
		configure_qrng = false;
	if (configure_qrng) {
		error = msm_rng_enable_hw(msm_rng_dev);
		if (error)
			goto rollback_clk;
	}

	mutex_init(&msm_rng_dev->rng_lock);
	mutex_init(&cached_rng_lock);

	/* register with hwrng framework */
	msm_rng.priv = (unsigned long) msm_rng_dev;
	error = hwrng_register(&msm_rng);
	if (error) {
		dev_err(&pdev->dev, "failed to register hwrng\n");
		error = -EPERM;
		goto rollback_clk;
	}
	ret = register_chrdev(QRNG_IOC_MAGIC, DRIVER_NAME, &msm_rng_fops);

	msm_rng_class = class_create(THIS_MODULE, "msm-rng");
	if (IS_ERR(msm_rng_class)) {
		pr_err("class_create failed\n");
		return PTR_ERR(msm_rng_class);
	}

	dev = device_create(msm_rng_class, NULL, MKDEV(QRNG_IOC_MAGIC, 0),
				NULL, "msm-rng");
	if (IS_ERR(dev)) {
		pr_err("Device create failed\n");
		error = PTR_ERR(dev);
		goto unregister_chrdev;
	}
	cdev_init(&msm_rng_cdev, &msm_rng_fops);
	msm_rng_dev_cached = msm_rng_dev;
	return error;

unregister_chrdev:
	unregister_chrdev(QRNG_IOC_MAGIC, DRIVER_NAME);
rollback_clk:
	clk_put(msm_rng_dev->prng_clk);
err_clk_get:
	iounmap(msm_rng_dev->base);
err_iomap:
	kzfree(msm_rng_dev);
err_exit:
	return error;
}

static int msm_rng_remove(struct platform_device *pdev)
{
	struct msm_rng_device *msm_rng_dev = platform_get_drvdata(pdev);

	unregister_chrdev(QRNG_IOC_MAGIC, DRIVER_NAME);
	hwrng_unregister(&msm_rng);
	clk_put(msm_rng_dev->prng_clk);
	iounmap(msm_rng_dev->base);
	platform_set_drvdata(pdev, NULL);
	if (msm_rng_dev->qrng_perf_client)
		msm_bus_scale_unregister_client(msm_rng_dev->qrng_perf_client);

	kzfree(msm_rng_dev);
	msm_rng_dev_cached = NULL;
	return 0;
}

static int qrng_get_random(struct crypto_rng *tfm, const u8 *src,
				unsigned int slen, u8 *rdata,
				unsigned int dlen)
{
	int sizeread = 0;
	int rv = -EFAULT;

	if (!msm_rng_dev_cached) {
		pr_err("%s: msm_rng_dev is not initialized.\n", __func__);
		rv = -ENODEV;
		goto err_exit;
	}

	if (!rdata) {
		pr_err("%s: data buffer is null!\n", __func__);
		rv = -EINVAL;
		goto err_exit;
	}

	if (signal_pending(current) ||
		mutex_lock_interruptible(&cached_rng_lock)) {
		pr_err("%s: mutex lock interrupted!\n", __func__);
		rv = -ERESTARTSYS;
		goto err_exit;
	}
	sizeread = msm_rng_direct_read(msm_rng_dev_cached, rdata, dlen);

	if (sizeread == dlen)
		rv = 0;

	mutex_unlock(&cached_rng_lock);
err_exit:
	return rv;

}

static int qrng_reset(struct crypto_rng *tfm, const u8 *seed, unsigned int slen)
{
	return 0;
}

static struct rng_alg rng_algs[] = { {
	.generate	= qrng_get_random,
	.seed		= qrng_reset,
	.seedsize	= 0,
	.base		= {
		.cra_name		= "qrng",
		.cra_driver_name	= "fips_hw_qrng",
		.cra_priority		= 300,
		.cra_ctxsize		= 0,
		.cra_module		= THIS_MODULE,
	}
} };

static const struct of_device_id qrng_match[] = {
	{	.compatible = "qcom,msm-rng",
	},
	{}
};

static struct platform_driver rng_driver = {
	.probe      = msm_rng_probe,
	.remove     = msm_rng_remove,
	.driver     = {
		.name   = DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = qrng_match,
	}
};

static int __init msm_rng_init(void)
{
	int ret;

	msm_rng_dev_cached = NULL;
	ret = platform_driver_register(&rng_driver);
	if (ret) {
		pr_err("%s: platform_driver_register error:%d\n",
			__func__, ret);
		goto err_exit;
	}
	ret = crypto_register_rngs(rng_algs, ARRAY_SIZE(rng_algs));
	if (ret) {
		pr_err("%s: crypto_register_algs error:%d\n",
			__func__, ret);
		goto err_exit;
	}

err_exit:
	return ret;
}

module_init(msm_rng_init);

static void __exit msm_rng_exit(void)
{
	crypto_unregister_rngs(rng_algs, ARRAY_SIZE(rng_algs));
	platform_driver_unregister(&rng_driver);
}

module_exit(msm_rng_exit);

MODULE_DESCRIPTION("QTI MSM Random Number Driver");
MODULE_LICENSE("GPL v2");
