/*
 * Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <mach/msm_bus.h>
#include <linux/qrng.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/platform_data/qcom_crypto_device.h>

#include "msm_rng.h"
#include "ctr_drbg.h"
#include "fips_drbg.h"
#include "msm_fips_selftest.h"

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

/* Global FIPS status  */
#ifdef CONFIG_FIPS_ENABLE
enum fips_status g_fips140_status = FIPS140_STATUS_FAIL;
EXPORT_SYMBOL(g_fips140_status);

#else
enum fips_status g_fips140_status = FIPS140_STATUS_NA;
EXPORT_SYMBOL(g_fips140_status);

#endif

/*FIPS140-2 call back for DRBG self test */
void *drbg_call_back;
EXPORT_SYMBOL(drbg_call_back);



enum {
	FIPS_NOT_STARTED = 0,
	DRBG_FIPS_STARTED
};

struct msm_rng_device msm_rng_device_info;

#ifdef CONFIG_FIPS_ENABLE
static int fips_mode_enabled = FIPS_NOT_STARTED;
#endif

static long msm_rng_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	long ret = 0;

	pr_debug("ioctl: cmd = %d\n", cmd);
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
int msm_rng_direct_read(struct msm_rng_device *msm_rng_dev, void *data)
{
	struct platform_device *pdev;
	void __iomem *base;
	size_t currsize = 0;
	unsigned long val;
	unsigned long *retdata = data;
	int ret;

	pdev = msm_rng_dev->pdev;
	base = msm_rng_dev->base;

	/* enable PRNG clock */
	ret = clk_prepare_enable(msm_rng_dev->prng_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clock in callback\n");
		return 0;
	}
	/* read random data from h/w */
	do {
		/* check status bit if data is available */
		if (!(readl_relaxed(base + PRNG_STATUS_OFFSET) & 0x00000001))
			break;	/* no data to read so just bail */

		/* read FIFO */
		val = readl_relaxed(base + PRNG_DATA_OUT_OFFSET);
		if (!val)
			break;	/* no data to read so just bail */

		/* write data back to callers pointer */
		*(retdata++) = val;
		currsize += 4;

	} while (currsize < Q_HW_DRBG_BLOCK_BYTES);

	/* vote to turn off clock */
	clk_disable_unprepare(msm_rng_dev->prng_clk);

	val = 0L;
	return currsize;

}

static int msm_rng_drbg_read(struct hwrng *rng,
			void *data, size_t max, bool wait)
{
	struct msm_rng_device *msm_rng_dev;
	struct platform_device *pdev;
	void __iomem *base;
	size_t maxsize;
	size_t currsize = 0;
	unsigned long val;
	unsigned long *retdata = data;
	int ret, ret1;

	msm_rng_dev = (struct msm_rng_device *)rng->priv;
	pdev = msm_rng_dev->pdev;
	base = msm_rng_dev->base;


	down(&msm_rng_dev->drbg_sem);

	/* calculate max size bytes to transfer back to caller */
	maxsize = min_t(size_t, MAX_HW_FIFO_SIZE, max);

	/* no room for word data */
	if (maxsize < 4)
		return 0;

	/* read random data from CTR-AES based DRBG */
	if (FIPS140_DRBG_ENABLED == msm_rng_dev->fips140_drbg_enabled) {
		ret1 = fips_drbg_gen(msm_rng_dev->drbg_ctx, data, maxsize);
		if (FIPS140_PRNG_ERR == ret1)
			panic("random number generator generator error.\n");
	} else
		ret1 = 1;

	/* read random data from h/w */
	/* enable PRNG clock */
	ret = clk_prepare_enable(msm_rng_dev->prng_clk);
	if (ret) {
		dev_err(&pdev->dev, "failed to enable clock in callback\n");
		up(&msm_rng_dev->drbg_sem);
		return 0;
	}
	/* read random data from h/w */
	do {
		/* check status bit if data is available */
		if (!(readl_relaxed(base + PRNG_STATUS_OFFSET) & 0x00000001))
			break;	/* no data to read so just bail */

		/* read FIFO */
		val = readl_relaxed(base + PRNG_DATA_OUT_OFFSET);
		if (!val)
			break;	/* no data to read so just bail */

		/* write data back to callers pointer */
		if (0 != ret1)
			*(retdata++) = val;
		currsize += 4;

		/* make sure we stay on 32bit boundary */
		if ((maxsize - currsize) < 4)
			break;
	} while (currsize < maxsize);
	/* vote to turn off clock */
	clk_disable_unprepare(msm_rng_dev->prng_clk);

	up(&msm_rng_dev->drbg_sem);

	return currsize;
}

#ifdef CONFIG_FIPS_ENABLE
static void _fips_drbg_init_error(struct msm_rng_device  *msm_rng_dev)
{
	unregister_chrdev(QRNG_IOC_MAGIC, DRIVER_NAME);
	clk_put(msm_rng_dev->prng_clk);
	iounmap(msm_rng_dev->base);
	kzfree(msm_rng_dev->drbg_ctx);
	kzfree(msm_rng_dev);
	panic("software random number generator initialization error.\n");
}
#else
static inline void _fips_drbg_init_error(struct msm_rng_device *msm_rng_dev)
{
	return;
}

#endif

#ifdef CONFIG_FIPS_ENABLE
int _do_msm_fips_drbg_init(void *rng_dev)
{
	struct msm_rng_device *msm_rng_dev = (struct msm_rng_device *) rng_dev;

	int ret;

	if (NULL == msm_rng_dev)
		return 1;

	ret = fips_drbg_init(msm_rng_dev);
	if (0 == ret) {
		pr_debug("start fips self test\n");
		ret = fips_self_test();
		if (ret) {
			msm_rng_dev->fips140_drbg_enabled =
				FIPS140_DRBG_DISABLED;
			_fips_drbg_init_error(msm_rng_dev);
		} else {
			msm_rng_dev->fips140_drbg_enabled =
				FIPS140_DRBG_ENABLED;
		}
	} else {
		msm_rng_dev->fips140_drbg_enabled = FIPS140_DRBG_DISABLED;
		_fips_drbg_init_error(msm_rng_dev);
	}

	return ret;
}
#else
int _do_msm_fips_drbg_init(void *rng_dev)
{
	return 0;
}
#endif

#ifdef CONFIG_FIPS_ENABLE
static int msm_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	struct msm_rng_device *msm_rng_dev = (struct msm_rng_device *)rng->priv;
	unsigned char a[Q_HW_DRBG_BLOCK_BYTES];
	int read_size;
	unsigned char *p = data;

	switch (fips_mode_enabled) {
	case DRBG_FIPS_STARTED:
		return msm_rng_drbg_read(rng, data, max, wait);
		break;
	case FIPS_NOT_STARTED:
		if (g_fips140_status != FIPS140_STATUS_PASS) {
			do {
				read_size = msm_rng_direct_read(msm_rng_dev, a);
				if (read_size <= 0)
					break;
				if ((max - read_size > 0)) {
					memcpy(p, a, read_size);
					p += read_size;
					max -= read_size;
				} else {
					memcpy(p, a, max);
				break;
				}
			} while (1);
			return p - (unsigned char *)data;
		} else {
				fips_mode_enabled  = DRBG_FIPS_STARTED;
				return msm_rng_drbg_read(rng, data, max, wait);
			}
		break;
	default:
		return 0;
		break;
	}

	return 0;
}
#else
static int msm_rng_read(struct hwrng *rng, void *data, size_t max, bool wait)
{
	return msm_rng_drbg_read(rng, data, max, wait);
}
#endif

static struct hwrng msm_rng = {
	.name = DRIVER_NAME,
	.read = msm_rng_read,
};

static int __devinit msm_rng_enable_hw(struct msm_rng_device *msm_rng_dev)
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
	return 0;
}

static const struct file_operations msm_rng_fops = {
	.unlocked_ioctl = msm_rng_ioctl,
};
static struct class *msm_rng_class;
static struct cdev msm_rng_cdev;

#ifdef CONFIG_FIPS_ENABLE

static void _first_msm_drbg_init(struct msm_rng_device *msm_rng_dev)
{
	fips_reg_drbg_callback((void *)msm_rng_dev);
	return;
}
#else
static void _first_msm_drbg_init(struct msm_rng_device *msm_rng_dev)
{
	_do_msm_fips_drbg_init(msm_rng_dev);
}
#endif

static int __devinit msm_rng_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct msm_rng_device *msm_rng_dev = NULL;
	void __iomem *base = NULL;
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

	msm_rng_dev->drbg_ctx = kzalloc(sizeof(struct fips_drbg_ctx_s),
					GFP_KERNEL);
	if (!msm_rng_dev->drbg_ctx) {
		dev_err(&pdev->dev, "cannot allocate memory\n");
		error = -ENOMEM;
		goto err_clk_get;
	}

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

	/* Enable rng h/w */
	error = msm_rng_enable_hw(msm_rng_dev);

	if (error)
		goto rollback_clk;

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

	sema_init(&msm_rng_dev->drbg_sem, 1);

	_first_msm_drbg_init(msm_rng_dev);

	return error;

unregister_chrdev:
	unregister_chrdev(QRNG_IOC_MAGIC, DRIVER_NAME);
rollback_clk:
	clk_put(msm_rng_dev->prng_clk);
err_clk_get:
	iounmap(msm_rng_dev->base);
err_iomap:
	kzfree(msm_rng_dev->drbg_ctx);
	kzfree(msm_rng_dev);
err_exit:
	return error;
}

static int __devexit msm_rng_remove(struct platform_device *pdev)
{
	struct msm_rng_device *msm_rng_dev = platform_get_drvdata(pdev);

	fips_drbg_final(msm_rng_dev->drbg_ctx);

	unregister_chrdev(QRNG_IOC_MAGIC, DRIVER_NAME);
	hwrng_unregister(&msm_rng);
	clk_put(msm_rng_dev->prng_clk);
	iounmap(msm_rng_dev->base);
	platform_set_drvdata(pdev, NULL);
	if (msm_rng_dev->qrng_perf_client)
		msm_bus_scale_unregister_client(msm_rng_dev->qrng_perf_client);
	if (msm_rng_dev->drbg_ctx) {
		kzfree(msm_rng_dev->drbg_ctx);
		msm_rng_dev->drbg_ctx = NULL;
	}
	kzfree(msm_rng_dev);
	return 0;
}

static struct of_device_id qrng_match[] = {
	{	.compatible = "qcom,msm-rng",
	},
	{}
};

static struct platform_driver rng_driver = {
	.probe      = msm_rng_probe,
	.remove     = __devexit_p(msm_rng_remove),
	.driver     = {
		.name   = DRIVER_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = qrng_match,
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
#ifdef CONFIG_FIPS_ENABLE
EXPORT_SYMBOL(fips_ctraes128_df_known_answer_test);
#endif
EXPORT_SYMBOL(_do_msm_fips_drbg_init);

MODULE_AUTHOR("The Linux Foundation");
MODULE_DESCRIPTION("Qualcomm MSM Random Number Driver");
MODULE_LICENSE("GPL v2");
